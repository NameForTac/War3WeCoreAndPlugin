#include "terrain_renderer_base.h"
#include "../src/core/map_builder.h"
#include "../src/core/slk.h"
#include "../plugins/terrain_edit/blp_reader.h"
#include <QOpenGLContext>
#include <QtMath>
#include <QApplication>
#include <QFile>
#include <QDir>
#include <algorithm>
#include <cmath>

// ============================================================
// GLSL shaders — VBO-based terrain rendering (#330, no SSBOs)
// ============================================================
static const char* kTerrainVert = R"(#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in float a_tex_layer;
uniform mat4 u_mvp;
out vec2 v_uv;
flat out float v_tex_layer;
void main() {
    gl_Position = u_mvp * vec4(a_pos, 1.0);
    v_uv = a_uv;
    v_tex_layer = a_tex_layer;
}
)";
static const char* kTerrainFrag = R"(#version 330 core
uniform sampler2DArray u_tex_array;
uniform float u_use_tex;
uniform vec3 u_light_dir;
uniform float u_lighting;
in vec2 v_uv;
flat in float v_tex_layer;
out vec4 frag_color;
void main() {
    vec4 color;
    if (u_use_tex > 0.5)
        color = texture(u_tex_array, vec3(v_uv, v_tex_layer));
    else
        color = vec4(0.4, 0.6, 0.3, 1.0);
    float light = mix(1.0, 0.3 + 0.7 * max(0.0, dot(normalize(u_light_dir), vec3(0.0, 1.0, 0.0))), u_lighting);
    frag_color = vec4(color.rgb * light, 1.0);
}
)";

static const char* kGridVert = R"(#version 330 core
layout(location = 0) in vec3 a_pos;
uniform mat4 u_mvp;
void main() { gl_Position = u_mvp * vec4(a_pos, 1.0); }
)";

static const char* kGridFrag = R"(
#version 330 core
uniform vec4 u_color;
out vec4 frag_color;
void main() { frag_color = u_color; }
)";

// ============================================================
// Static helpers: SLK parsing and BLP loading
// ============================================================
static std::vector<uint8_t> readLocalSlk(const char* filename) {
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/../../Data/" + filename,
        QCoreApplication::applicationDirPath() + "/../Data/" + filename,
        QCoreApplication::applicationDirPath() + "/Data/" + filename,
        QDir::currentPath() + "/Data/" + filename,
    };
    for (auto& path : candidates) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            auto bytes = f.readAll(); f.close();
            return {bytes.begin(), bytes.end()};
        }
    }
    return {};
}

static QImage decodeTerrainTex(const std::vector<uint8_t>& data) {
    if (data.size() >= 4 && data[0] == 'B' && data[1] == 'L' && data[2] == 'P' && data[3] == '1')
        return read_blp(data);
    QImage img;
    img.loadFromData(data.data(), (int)data.size());
    return img;
}

static QHash<QString, QString> loadTilePathMap(MapBuilder* builder) {
    static QHash<QString, QString> s_cache;
    static bool s_tried = false;
    if (s_tried) return s_cache;
    s_tried = true;

    std::vector<uint8_t> data = readLocalSlk("Terrain.slk");
    if (data.empty() && builder)
        data = builder->read_resource("TerrainArt\\Terrain.slk");
    if (data.empty()) return s_cache;

    std::string text(data.begin(), data.end());
    auto table = slk::parse(text);
    for (size_t r = 0; r < table.rows.size(); ++r) {
        auto& row = table.rows[r];
        if (row.size() < 4) continue;
        auto& tileId = row[0];
        auto& dir    = row[1];
        auto& file   = row[2];
        if (tileId.empty() || dir.empty() || file.empty()) continue;
        s_cache.insert(QString::fromStdString(tileId),
                       QString::fromStdString(dir + "\\" + file + ".blp"));
    }
    return s_cache;
}

// ============================================================
// Constructor / Destructor
// ============================================================
TerrainRendererBase::TerrainRendererBase(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMouseTracking(true);
}

TerrainRendererBase::~TerrainRendererBase() {
    makeCurrent();
    destroyGPUBuffers();
    if (vao_)           glDeleteVertexArrays(1, &vao_);
    if (program_)       glDeleteProgram(program_);
    if (grid_program_)  glDeleteProgram(grid_program_);
    if (tex_array_)     glDeleteTextures(1, &tex_array_);
    doneCurrent();
}

// ============================================================
// Debug helper — check GL errors (called from member functions)
// ============================================================
#define DBG_GL(msg) do { \
    GLenum _e = glGetError(); \
    if (_e != GL_NO_ERROR) \
        qWarning() << "GL error 0x" + QString::number(_e, 16) << "at" << msg; \
} while(0)

// ============================================================
// Shader compilation
// ============================================================
GLuint TerrainRendererBase::compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        // Print first 80 chars of source for debugging
        QString srcPreview = QString::fromUtf8(src).left(80).replace('\n', ' ');
        qWarning() << "TerrainRendererBase: shader compile error:" << log
                    << "src_preview:" << srcPreview;
        return 0;
    }
    return shader;
}

GLuint TerrainRendererBase::linkProgram(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        qWarning() << "TerrainRendererBase: program link error:" << log;
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

bool TerrainRendererBase::initTerrainShaders() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, kTerrainVert);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kTerrainFrag);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    program_ = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!program_) return false;

    u_mvp_       = glGetUniformLocation(program_, "u_mvp");
    u_light_dir_ = glGetUniformLocation(program_, "u_light_dir");
    u_lighting_  = glGetUniformLocation(program_, "u_lighting");
    u_use_tex_   = glGetUniformLocation(program_, "u_use_tex");
    u_tex_array_ = glGetUniformLocation(program_, "u_tex_array");
    return true;
}

void TerrainRendererBase::initGridShaders() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, kGridVert);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kGridFrag);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return;
    }
    grid_program_ = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (grid_program_)
        u_grid_mvp_ = glGetUniformLocation(grid_program_, "u_mvp");
}

// ============================================================
// Texture: tile texture with cliff/blight awareness
// ============================================================
uint8_t TerrainRendererBase::realTileTexture(int x, int y) const {
    if (!terrain_) return 0;
    int w = terrain_->tile_width;
    size_t idx = (size_t)y * w + x;

    bool has_cliff = false;
    int cliff_tex = 0;

    for (int dy = -1; dy <= 0; ++dy) {
        for (int dx = -1; dx <= 0; ++dx) {
            int tx = x + dx, ty = y + dy;
            if (tx < 0 || ty < 0 || tx >= w - 1 || ty >= terrain_->tile_height - 1) continue;
            size_t bl = (size_t)ty * w + tx;
            size_t br = bl + 1, tl = bl + w, tr = bl + w + 1;
            uint8_t lh = terrain_->tilepoints[bl].layer_height;
            if (terrain_->tilepoints[br].layer_height != lh ||
                terrain_->tilepoints[tl].layer_height != lh ||
                terrain_->tilepoints[tr].layer_height != lh) {
                has_cliff = true;
                cliff_tex = terrain_->tilepoints[bl].cliff_texture;
                break;
            }
        }
        if (has_cliff) break;
    }

    if (has_cliff && cliff_tex < 16)
        return cliff_tex;

    const auto& tp = terrain_->tilepoints[idx];
    if (tp.flags & 2)
        return (uint8_t)std::max(0, (int)terrain_->ground_tiles.size() - 1);

    return tp.ground_texture;
}

int TerrainRendererBase::getTileVariant(int tex_idx, int variation) const {
    if (tex_idx < 0 || tex_idx >= (int)tex_extended_.size()) return 0;
    if (tex_extended_[tex_idx]) {
        if (variation <= 15) return 16 + variation;
        return 0;
    }
    return 0;
}

// ============================================================
// Procedural texture generation (fallback when BLP missing)
// ============================================================
QImage TerrainRendererBase::generateNoiseTexture(int w, int h,
                                                  const QColor& base,
                                                  const QColor& var, float scale)
{
    QImage img(w, h, QImage::Format_RGB888);
    auto hash = [](int a, int b) -> float {
        unsigned int h = (unsigned)a * 374761393u + (unsigned)b * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (h & 0x7FFFFFFF) / (float)0x7FFFFFFF;
    };
    for (int y = 0; y < h; ++y) {
        uchar* line = img.scanLine(y);
        float ny = (y / (float)h) * scale;
        int iy = (int)ny; float fy = ny - iy;
        for (int x = 0; x < w; ++x) {
            float nx = (x / (float)w) * scale;
            int ix = (int)nx; float fx = nx - ix;
            float n00 = hash(ix, iy), n10 = hash(ix+1, iy);
            float n01 = hash(ix, iy+1), n11 = hash(ix+1, iy+1);
            float n = (n00*(1-fx)+n10*fx)*(1-fy) + (n01*(1-fx)+n11*fx)*fy;
            int r = qBound(0, (int)(base.red()+var.red()*(n-0.5f)*2.0f), 255);
            int g = qBound(0, (int)(base.green()+var.green()*(n-0.5f)*2.0f), 255);
            int b = qBound(0, (int)(base.blue()+var.blue()*(n-0.5f)*2.0f), 255);
            bool grid = (x < 8 || x >= w - 8) || (y < 8 || y >= h - 8);
            if (grid) { r = r * 3 / 4; g = g * 3 / 4; b = b * 3 / 4; }
            line[x*3]=(uchar)r; line[x*3+1]=(uchar)g; line[x*3+2]=(uchar)b;
        }
    }
    return img;
}

QImage TerrainRendererBase::generateTileTexture(const QString& tileId, int idx) {
    QColor base, var;
    float scale = 6.0f;
    auto set = [&](int r,int g,int b,int vr,int vg,int vb) {
        base=QColor(r,g,b); var=QColor(vr,vg,vb);
    };
    if (tileId.length() >= 4) {
        QString type = tileId.mid(1, 3);
        if (type == "grs"||type == "pla")  set(70,145,50,40,50,25);
        else if (type == "grh")            set(55,125,40,35,45,20);
        else if (type == "drt")            set(155,130,85,40,35,30);
        else if (type == "dro"||type == "roc") set(140,115,70,35,30,25);
        else if (type == "rok")            set(120,115,105,30,25,25);
        else if (type == "sor"||type == "sqd") set(150,140,130,30,30,30);
        else if (type == "snw"||type == "hwa") set(195,205,215,30,25,20);
        else if (type == "dsr")            set(200,175,110,35,35,30);
        else if (type == "blp")            set(55,50,45,15,15,15);
        else if (type == "bbb")            set(130,100,70,30,25,20);
        else if (type == "clf")            set(105,95,72,25,20,20);
        else if (type == "wtf"||type == "gtf") set(150,145,135,25,25,25);
        else if (type == "hdg")            set(50,95,35,30,35,20);
        else if (type == "dos")            set(50,95,30,30,35,20);
        else                                set(130,120,100,40,40,40);
    } else { set(130,120,100,40,40,40); }
    return generateNoiseTexture(tile_size_, tile_size_, base, var, scale);
}

// ============================================================
// Texture loading from BLP / MPQ
// ============================================================
void TerrainRendererBase::uploadTextures() {
    if (!terrain_ || terrain_->ground_tiles.empty()) { tex_count_ = 0; return; }
    if (!isValid()) return;

    if (tex_array_) { glDeleteTextures(1, &tex_array_); tex_array_ = 0; }
    tex_count_ = (int)terrain_->ground_tiles.size();
    if (tex_count_ <= 0) {
        qWarning() << "TerrainRendererBase::uploadTextures: no ground tiles, abort";
        return;
    }
    qWarning() << "TerrainRendererBase::uploadTextures: loading" << tex_count_ << "tile textures"
               << "builder_=" << (void*)builder_;
    DBG_GL("uploadTextures-start");

    constexpr int kVariantsPerTile = 32;
    constexpr int kGridCols = 8, kGridRows = 4;

    std::vector<QImage> all_variants;
    all_variants.reserve(tex_count_ * kVariantsPerTile);
    layer_offsets_.resize(tex_count_);
    tex_extended_.resize(tex_count_);
    tile_size_ = 64;

    auto pathMap = loadTilePathMap(builder_);
    int loaded_from_blp = 0;

    for (int i = 0; i < tex_count_; ++i) {
        layer_offsets_[i] = i * kVariantsPerTile;
        bool got_blp = false;
        QString tileId = QString::fromStdString(terrain_->ground_tiles[i]);
        QString mpqPath;
        auto it = pathMap.find(tileId);
        if (it != pathMap.end()) mpqPath = it.value();

        if (builder_ && !mpqPath.isEmpty()) {
            std::string nativePath = mpqPath.toStdString();
            std::vector<uint8_t> raw = builder_->read_raw(nativePath);
            if (raw.empty()) {
                std::string fwd = nativePath;
                for (auto& c : fwd) if (c == '\\') c = '/';
                if (fwd != nativePath) raw = builder_->read_raw(fwd);
            }
            if (raw.empty()) {
                std::string tgaPath = nativePath.substr(0, nativePath.size()-4) + ".tga";
                raw = builder_->read_raw(tgaPath);
                if (raw.empty()) {
                    std::string tgaFwd = tgaPath;
                    for (auto& c : tgaFwd) if (c == '\\') c = '/';
                    if (tgaFwd != tgaPath) raw = builder_->read_raw(tgaFwd);
                }
            }
            if (raw.empty()) raw = builder_->read_resource(nativePath);
            if (!raw.empty()) {
                QImage blp = decodeTerrainTex(raw).convertToFormat(QImage::Format_RGB888);
                if (!blp.isNull()) {
                    ++loaded_from_blp; got_blp = true;
                    int w = blp.width(), h = blp.height();
                    int sub_size = std::max(h / kGridRows, 1);
                    int cols = std::min(w / sub_size, kGridCols);
                    tile_size_ = std::max(tile_size_, sub_size);
                    tex_extended_[i] = (cols > 4);
                    for (int vy = 0; vy < kGridRows; ++vy)
                        for (int vx = 0; vx < cols; ++vx)
                            all_variants.push_back(
                                blp.copy(vx*sub_size, vy*sub_size, sub_size, sub_size)
                                    .convertToFormat(QImage::Format_RGB888));
                    int have = (int)all_variants.size() - layer_offsets_[i];
                    for (int j = have; j < kVariantsPerTile; ++j)
                        all_variants.push_back(all_variants[all_variants.size()-have]);
                }
            }
        }
        if (!got_blp) {
            tex_extended_[i] = false;
            QImage proc = generateTileTexture(tileId, i)
                .scaled(tile_size_, tile_size_, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                .convertToFormat(QImage::Format_RGB888);
            for (int j = 0; j < kVariantsPerTile; ++j) all_variants.push_back(proc);
        }
    }

    for (auto& img : all_variants)
        if (img.width() != tile_size_ || img.height() != tile_size_)
            img = img.scaled(tile_size_, tile_size_, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    int total_layers = tex_count_ * kVariantsPerTile;

    glGenTextures(1, &tex_array_);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex_array_);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, tile_size_, tile_size_, total_layers,
                 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int layer = 0; layer < total_layers; ++layer) {
        if (all_variants[layer].isNull()) continue;
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, tile_size_, tile_size_, 1,
                        GL_RGB, GL_UNSIGNED_BYTE, all_variants[layer].bits());
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    DBG_GL("uploadTextures-end");
    qWarning() << "TerrainRendererBase::uploadTextures: done, tex_array_=" << tex_array_
               << "tile_size=" << tile_size_ << "total_layers=" << total_layers;
}

void TerrainRendererBase::freeTextures() {
    if (!isValid()) return;
    if (tex_array_) { glDeleteTextures(1, &tex_array_); tex_array_ = 0; tex_count_ = 0; }
}

// ============================================================
// GPU buffer management (VBO-based quad mesh, no SSBOs)
// ============================================================
void TerrainRendererBase::createGPUBuffers() {
    if (!terrain_) return;
    int w = terrain_->tile_width;
    int h = terrain_->tile_height;
    qWarning() << "TerrainRendererBase::createGPUBuffers:" << w << "x" << h
               << "quads=" << ((w-1)*(h-1));

    glGenVertexArrays(1, &terrain_vao_);
    glGenBuffers(1, &terrain_vbo_);
    terrain_vertex_count_ = 0;
    geometry_dirty_ = true;
    DBG_GL("createGPUBuffers");
}

void TerrainRendererBase::destroyGPUBuffers() {
    if (!isValid()) return;
    if (terrain_vao_) { glDeleteVertexArrays(1, &terrain_vao_); terrain_vao_ = 0; }
    if (terrain_vbo_) { glDeleteBuffers(1, &terrain_vbo_); terrain_vbo_ = 0; }
    terrain_vertex_count_ = 0;
}

void TerrainRendererBase::rebuildTerrainGeometry() {
    if (!terrain_ || !terrain_vao_ || !terrain_vbo_) return;
    int cols = terrain_->tile_width;
    int rows = terrain_->tile_height;
    int quads_x = cols - 1;
    int quads_y = rows - 1;
    if (quads_x <= 0 || quads_y <= 0) return;

    // Interleaved vertex format: pos3 + uv2 + texLayer1 = 6 floats = 24 bytes
    // 6 vertices per quad (2 triangles), quads_x * quads_y quads total
    int total_verts = quads_x * quads_y * 6;
    std::vector<float> verts;
    verts.reserve(total_verts * 6);

    for (int qy = 0; qy < quads_y; ++qy) {
        for (int qx = 0; qx < quads_x; ++qx) {
            // Corner heights from tilepoints (4 corners per quad)
            int idx_tl = qy * cols + qx;
            int idx_tr = qy * cols + qx + 1;
            int idx_bl = (qy + 1) * cols + qx;
            int idx_br = (qy + 1) * cols + qx + 1;

            float h_tl = rawToWorldY(terrain_->tilepoints[idx_tl].height);
            float h_tr = rawToWorldY(terrain_->tilepoints[idx_tr].height);
            float h_bl = rawToWorldY(terrain_->tilepoints[idx_bl].height);
            float h_br = rawToWorldY(terrain_->tilepoints[idx_br].height);

            float wx = terrain_->center_offset_x + qx * kTileSize;
            float wz = terrain_->center_offset_y + qy * kTileSize;
            float ws = kTileSize;

            // Texture layer: use top-left corner texture
            uint8_t tex = realTileTexture(qx, qy);
            int layer = (tex < (uint8_t)tex_count_ && tex < (uint8_t)layer_offsets_.size())
                        ? layer_offsets_[tex] : 0;

            float lt = (float)layer;

            // Two triangles forming a quad:
            // Tri 1: TL -> TR -> BL
            // Tri 2: TR -> BR -> BL
            // Vertex format: x, y, z, u, v, tex_layer  (6 floats)
            auto push = [&](float x, float y, float z, float u, float v) {
                verts.push_back(x); verts.push_back(y); verts.push_back(z);
                verts.push_back(u); verts.push_back(v); verts.push_back(lt);
            };

            // Triangle 1
            push(wx,        h_tl, wz,        0, 0);
            push(wx + ws,   h_tr, wz,        1, 0);
            push(wx,        h_bl, wz + ws,   0, 1);
            // Triangle 2
            push(wx + ws,   h_tr, wz,        1, 0);
            push(wx + ws,   h_br, wz + ws,   1, 1);
            push(wx,        h_bl, wz + ws,   0, 1);
        }
    }

    terrain_vertex_count_ = (int)verts.size() / 6;

    glBindVertexArray(terrain_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, terrain_vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);

    // a_pos (location 0): 3 floats
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // a_uv (location 1): 2 floats
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // a_tex_layer (location 2): 1 float
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    geometry_dirty_ = false;
    DBG_GL("rebuildTerrainGeometry");
}

// ============================================================
// OpenGL initialization
// ============================================================
void TerrainRendererBase::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Create dummy VAO — required in core profile for any draw call, even SSBO-based
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    // Log GL context info for debugging
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* vendor = glGetString(GL_VENDOR);
    qWarning() << "TerrainRendererBase::initializeGL:"
               << "renderer=" << (renderer ? (const char*)renderer : "?")
               << "version=" << (version ? (const char*)version : "?")
               << "vendor=" << (vendor ? (const char*)vendor : "?")
               << "vao=" << vao_;

    if (!initTerrainShaders())
        qWarning() << "TerrainRendererBase: terrain shader init failed!";
    else
        qWarning() << "TerrainRendererBase: terrain shaders OK, program=" << program_;
}

// ============================================================
// Rendering
// ============================================================
void TerrainRendererBase::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

QMatrix4x4 TerrainRendererBase::computeMVPMatrix(float aspect) const {
    float yaw = qDegreesToRadians(cam_yaw_);
    float pitch = qDegreesToRadians(cam_pitch_);
    float cosP = qCos(pitch);
    float sinP = qAbs(qSin(pitch));

    QVector3D eye(
        cam_center_.x() + cam_distance_ * qCos(yaw) * cosP,
        cam_distance_ * sinP,
        cam_center_.y() + cam_distance_ * qSin(yaw) * cosP);
    QVector3D center(cam_center_.x(), 0.0f, cam_center_.y());
    QVector3D up(0.0f, 1.0f, 0.0f);

    QMatrix4x4 proj;
    proj.perspective(45.0f, aspect, 1.0f, cam_distance_ * 4.0f);
    QMatrix4x4 view;
    view.lookAt(eye, center, up);
    return proj * view;
}

void TerrainRendererBase::renderTerrain(const QMatrix4x4& mvp) {
    if (!terrain_ || terrain_vertex_count_ <= 0) return;

    // Rebuild geometry if terrain data changed
    if (geometry_dirty_) {
        rebuildTerrainGeometry();
    }

    // Terrain is a flat mesh — disable back-face culling to avoid
    // winding-order issues after the perspective transform.
    glDisable(GL_CULL_FACE);

    glUseProgram(program_);
    glBindVertexArray(terrain_vao_);
    glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, mvp.constData());
    glUniform3f(u_light_dir_, 0.5f, 0.8f, 0.3f);
    glUniform1f(u_lighting_, 1.0f);

    bool useTex = show_texture_ && tex_array_ > 0 && tex_count_ > 0;
    glUniform1f(u_use_tex_, useTex ? 1.0f : 0.0f);

    if (tex_array_) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex_array_);
        glUniform1i(u_tex_array_, 0);
    }

    // Derived-class customization (wireframe, unlit)
    onBeforePaint();

    glDrawArrays(GL_TRIANGLES, 0, terrain_vertex_count_);
    DBG_GL("renderTerrain-draw");

    // Restore state for overlays
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
}

void TerrainRendererBase::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!has_terrain_ || !terrain_ || !program_) {
        qWarning() << "TerrainRendererBase::paintGL early return:"
                   << "has_terrain_=" << has_terrain_
                   << "terrain_=" << (void*)terrain_
                   << "program_=" << program_;
        return;
    }

    int w = terrain_->tile_width;
    int h = terrain_->tile_height;
    int num_tiles = (w - 1) * (h - 1);
    if (num_tiles <= 0) {
        qWarning() << "TerrainRendererBase::paintGL early return: no tiles"
                   << "w=" << w << "h=" << h << "num_tiles=" << num_tiles;
        return;
    }

    // Lazy texture + buffer init
    if (tex_dirty_) {
        qWarning() << "TerrainRendererBase::paintGL: textures dirty, loading..."
                   << "tex_count=" << (terrain_ ? (int)terrain_->ground_tiles.size() : -1);
        if (tex_array_) freeTextures();
        uploadTextures();
        tex_dirty_ = false;
        qWarning() << "TerrainRendererBase::paintGL: textures done, tex_array_=" << tex_array_
                   << "tex_count_=" << tex_count_;
    }
    if (!terrain_vao_) {
        qWarning() << "TerrainRendererBase::paintGL: creating GPU buffers...";
        createGPUBuffers();
        rebuildTerrainGeometry();
        qWarning() << "TerrainRendererBase::paintGL: buffers ready, verts=" << terrain_vertex_count_;
    }

    // Camera
    float aspect = static_cast<float>(width()) / static_cast<float>(qMax(height(), 1));
    last_mvp_ = computeMVPMatrix(aspect);

    // Draw terrain (includes onBeforePaint hook)
    renderTerrain(last_mvp_);

    // Derived-class overlay rendering
    onAfterPaint();

    glUseProgram(0);
}

// ============================================================
// Mouse / camera controls (orbit, pan, zoom)
// ============================================================
void TerrainRendererBase::baseMousePress(QMouseEvent* event) {
    dragging_ = true;
    drag_button_ = event->button();
    last_mouse_pos_ = event->position();
}

void TerrainRendererBase::baseMouseMove(QMouseEvent* event) {
    if (!dragging_ || !has_terrain_) return;
    QPointF pos = event->position();
    float dx = static_cast<float>(pos.x() - last_mouse_pos_.x());
    float dy = static_cast<float>(pos.y() - last_mouse_pos_.y());
    last_mouse_pos_ = pos;

    if (drag_button_ == Qt::MiddleButton ||
        (drag_button_ == Qt::LeftButton && QApplication::keyboardModifiers() & Qt::AltModifier)) {
        cam_yaw_ += dx * 0.3f;
        cam_pitch_ += dy * 0.3f;
        cam_pitch_ = qBound(-89.0f, cam_pitch_, 89.0f);
        update();
    } else if (drag_button_ == Qt::RightButton) {
        float scale = cam_distance_ * 0.002f;
        float yaw = qDegreesToRadians(cam_yaw_);
        QVector3D forward(qCos(yaw), 0, qSin(yaw));
        QVector3D right(-qSin(yaw), 0, qCos(yaw));
        forward.normalize(); right.normalize();
        cam_center_ += QPointF(right.x() * dx * scale, right.z() * dx * scale);
        cam_center_ -= QPointF(forward.x() * dy * scale, forward.z() * dy * scale);
        update();
    }
}

void TerrainRendererBase::baseMouseRelease(QMouseEvent* /*event*/) {
    dragging_ = false;
}

void TerrainRendererBase::baseWheelEvent(QWheelEvent* event) {
    float delta = static_cast<float>(event->angleDelta().y()) / 120.0f;
    cam_distance_ *= (1.0f - delta * 0.1f);
    cam_distance_ = qBound(10.0f, cam_distance_, 100000.0f);
    update();
}

// ============================================================
// Default event handlers: derived classes override for editing
// ============================================================
void TerrainRendererBase::mousePressEvent(QMouseEvent* event) {
    if (!has_terrain_) return;
    baseMousePress(event);
    onMousePress(event);
}

void TerrainRendererBase::mouseMoveEvent(QMouseEvent* event) {
    baseMouseMove(event);
    onMouseMove(event);
}

void TerrainRendererBase::mouseReleaseEvent(QMouseEvent* event) {
    baseMouseRelease(event);
    onMouseRelease(event);
}

void TerrainRendererBase::wheelEvent(QWheelEvent* event) {
    if (!has_terrain_) return;
    baseWheelEvent(event);
}
