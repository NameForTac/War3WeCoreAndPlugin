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
// GLSL 430 core shaders — GPU-driven instanced terrain
// ============================================================
static const char* kTerrainVert = R"(
#version 430 core

layout(location = 0) uniform mat4 u_mvp;
layout(location = 1) uniform ivec2 u_map_size;
layout(location = 5) uniform vec2 u_origin;

const float kTileSize = 128.0;

layout(std430, binding = 0) buffer HeightBuf { float heights[]; };
layout(std430, binding = 1) buffer CliffLevelBuf { float cliff_levels[]; };
layout(std430, binding = 2) buffer TextureDataBuf { uvec4 tex_data[]; };

layout(location = 0) out vec2 v_uv;
layout(location = 1) out flat ivec4 v_tex;
layout(location = 2) out vec3 v_normal;

const vec2 quad[6] = vec2[6](
    vec2(1.0, 1.0), vec2(0.0, 1.0), vec2(0.0, 0.0),
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
);

void main() {
    ivec2 tile_pos = ivec2(gl_InstanceID % (u_map_size.x - 1),
                           gl_InstanceID / (u_map_size.x - 1));
    vec2 lv = quad[gl_VertexID];
    ivec2 c = ivec2(lv) + tile_pos;
    int idx = c.y * u_map_size.x + c.x;
    float h = cliff_levels[idx];

    float hL = heights[c.y * u_map_size.x + max(c.x - 1, 0)];
    float hR = heights[c.y * u_map_size.x + min(c.x + 1, u_map_size.x - 1)];
    float hD = heights[max(c.y - 1, 0) * u_map_size.x + c.x];
    float hU = heights[min(c.y + 1, u_map_size.y - 1) * u_map_size.x + c.x];
    v_normal = normalize(vec3(hL - hR, 2.0 * kTileSize, hD - hU));

    int tile_idx = tile_pos.y * (u_map_size.x - 1) + tile_pos.x;
    v_tex = ivec4(tex_data[tile_idx]);
    v_uv = lv;
    vec2 world_pos = (lv + vec2(tile_pos)) * kTileSize + u_origin;
    gl_Position = u_mvp * vec4(world_pos.x, h, world_pos.y, 1.0);
}
)";

static const char* kTerrainFrag = R"(
#version 430 core

layout(location = 2) uniform float u_lighting;
layout(location = 3) uniform vec3  u_light_dir;
layout(location = 4) uniform float u_use_tex;
uniform sampler2DArray u_tex_array;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in flat ivec4 v_tex;
layout(location = 2) in vec3 v_normal;

layout(location = 0) out vec4 frag_color;

void main() {
    vec2 t = v_uv;
    float wBL = (1.0 - t.x) * (1.0 - t.y);
    float wBR = t.x * (1.0 - t.y);
    float wTL = (1.0 - t.x) * t.y;
    float wTR = t.x * t.y;

    vec3 color;
    if (u_use_tex > 0.5) {
        vec3 c0 = texture(u_tex_array, vec3(t, float(v_tex[0]))).rgb;
        vec3 c1 = texture(u_tex_array, vec3(t, float(v_tex[1]))).rgb;
        vec3 c2 = texture(u_tex_array, vec3(t, float(v_tex[2]))).rgb;
        vec3 c3 = texture(u_tex_array, vec3(t, float(v_tex[3]))).rgb;
        color = c0 * wBL + c1 * wBR + c2 * wTL + c3 * wTR;
    } else {
        color = vec3(0.4, 0.6, 0.3);
    }

    vec3 N = normalize(v_normal);
    float diff = max(dot(N, u_light_dir), 0.0);
    float ambient = 0.3;
    float lighting = ambient + (1.0 - ambient) * diff;
    color = mix(color, color * lighting, u_lighting);

    frag_color = vec4(color, 1.0);
}
)";

static const char* kGridVert = R"(
#version 330 core
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
    if (program_)       glDeleteProgram(program_);
    if (grid_program_)  glDeleteProgram(grid_program_);
    if (tex_array_)     glDeleteTextures(1, &tex_array_);
    doneCurrent();
}

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
        qWarning() << "TerrainRendererBase: shader compile error:" << log;
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
    u_map_size_  = glGetUniformLocation(program_, "u_map_size");
    u_light_dir_ = glGetUniformLocation(program_, "u_light_dir");
    u_lighting_  = glGetUniformLocation(program_, "u_lighting");
    u_use_tex_   = glGetUniformLocation(program_, "u_use_tex");
    u_tex_array_ = glGetUniformLocation(program_, "u_tex_array");
    u_origin_    = glGetUniformLocation(program_, "u_origin");
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
    if (tex_count_ <= 0) return;

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
}

void TerrainRendererBase::freeTextures() {
    if (!isValid()) return;
    if (tex_array_) { glDeleteTextures(1, &tex_array_); tex_array_ = 0; tex_count_ = 0; }
}

// ============================================================
// GPU buffer management (HiveWE-style SSBO)
// ============================================================
void TerrainRendererBase::createGPUBuffers() {
    if (!terrain_) return;
    int w = terrain_->tile_width;
    int h = terrain_->tile_height;

    gpu_heights_.resize(w * h);
    gpu_final_heights_.resize(w * h);
    gpu_texture_data_.resize((w - 1) * (h - 1));

    glGenBuffers(1, &height_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, height_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, w * h * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &cliff_level_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cliff_level_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, w * h * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &texture_data_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, texture_data_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (w - 1) * (h - 1) * sizeof(TileTextureGPU), nullptr, GL_DYNAMIC_DRAW);
}

void TerrainRendererBase::destroyGPUBuffers() {
    if (!isValid()) return;
    if (vao_)               { glDeleteVertexArrays(1, &vao_);          vao_ = 0; }
    if (height_ssbo_)       { glDeleteBuffers(1, &height_ssbo_);       height_ssbo_ = 0; }
    if (cliff_level_ssbo_)  { glDeleteBuffers(1, &cliff_level_ssbo_);  cliff_level_ssbo_ = 0; }
    if (texture_data_ssbo_) { glDeleteBuffers(1, &texture_data_ssbo_); texture_data_ssbo_ = 0; }
}

void TerrainRendererBase::updateGroundHeights(const QRect& area) {
    if (!terrain_) return;
    int w = terrain_->tile_width;

    for (int y = area.y(); y < area.y() + area.height() && y < terrain_->tile_height; ++y) {
        for (int x = area.x(); x < area.x() + area.width() && x < terrain_->tile_width; ++x) {
            size_t idx = (size_t)y * w + x;
            float raw_h = terrain_->tilepoints[idx].height;
            gpu_heights_[idx] = raw_h;
            gpu_final_heights_[idx] = (raw_h - 8192.0f) * 0.25f;
        }
    }

    size_t byte_count = (size_t)terrain_->tile_height * w * sizeof(float);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, height_ssbo_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, byte_count, gpu_heights_.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cliff_level_ssbo_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, byte_count, gpu_final_heights_.data());
}

void TerrainRendererBase::updateGroundTextures(const QRect& area) {
    if (!terrain_) return;
    int w = terrain_->tile_width;
    int h = terrain_->tile_height;
    int quads_x = w - 1;
    int quads_y = h - 1;

    QRect safe = area.adjusted(-1, -1, 1, 1).intersected({0, 0, w, h});

    for (int y = safe.y(); y < safe.y() + safe.height() - 1 && y < quads_y; ++y) {
        for (int x = safe.x(); x < safe.x() + safe.width() - 1 && x < quads_x; ++x) {
            uint8_t corner_tex[4] = {
                realTileTexture(x, y),
                realTileTexture(x + 1, y),
                realTileTexture(x, y + 1),
                realTileTexture(x + 1, y + 1)
            };

            TileTextureGPU out{};
            for (int c = 0; c < 4; ++c) {
                if (corner_tex[c] < (uint8_t)tex_count_) {
                    int tex = corner_tex[c];
                    int corner_x = x + (c & 1);
                    int corner_y = y + (c >> 1);
                    int var = 0;
                    if (corner_x < w && corner_y < h) {
                        size_t idx = (size_t)corner_y * w + corner_x;
                        var = getTileVariant(tex, terrain_->tilepoints[idx].texture_detail & 0x1F);
                    }
                    out.layers[c] = (uint32_t)(layer_offsets_[tex] + var);
                } else {
                    out.layers[c] = 0xFFFFu;
                }
            }
            gpu_texture_data_[(size_t)y * quads_x + x] = out;
        }
    }

    size_t byte_count = (size_t)quads_y * quads_x * sizeof(TileTextureGPU);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, texture_data_ssbo_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, byte_count, gpu_texture_data_.data());
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

    if (!initTerrainShaders())
        qWarning() << "TerrainRendererBase: terrain shader init failed!";
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
    glUseProgram(program_);
    glBindVertexArray(vao_);
    glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, mvp.constData());
    glUniform2i(u_map_size_, terrain_->tile_width, terrain_->tile_height);
    glUniform2f(u_origin_, terrain_->center_offset_x, terrain_->center_offset_y);
    glUniform3f(u_light_dir_, 0.5f, 0.8f, 0.3f);
    glUniform1f(u_lighting_, 1.0f);

    bool useTex = show_texture_ && tex_array_ > 0 && tex_count_ > 0;
    glUniform1f(u_use_tex_, useTex ? 1.0f : 0.0f);

    if (tex_array_) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex_array_);
        glUniform1i(u_tex_array_, 0);
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, height_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, cliff_level_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, texture_data_ssbo_);

    // Derived-class customization (wireframe, unlit)
    onBeforePaint();

    int num_tiles = (terrain_->tile_width - 1) * (terrain_->tile_height - 1);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, num_tiles);

    // Restore state for overlays
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void TerrainRendererBase::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!has_terrain_ || !terrain_ || !program_) return;

    int w = terrain_->tile_width;
    int h = terrain_->tile_height;
    int num_tiles = (w - 1) * (h - 1);
    if (num_tiles <= 0) return;

    // Lazy texture + buffer init
    if (tex_dirty_) {
        if (tex_array_) freeTextures();
        uploadTextures();
        tex_dirty_ = false;
    }
    if (!height_ssbo_) {
        createGPUBuffers();
        updateGroundHeights({0, 0, w, h});
        updateGroundTextures({0, 0, w, h});
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
