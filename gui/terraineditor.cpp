#include "terraineditor.h"
#include "../src/core/map_builder.h"
#include "../src/core/slk.h"
#include "../plugins/terrain_edit/blp_reader.h"
#include <QOpenGLContext>
#include <QPainter>
#include <QtMath>
#include <QApplication>
#include <QFile>
#include <QDir>
#include <algorithm>
#include <cmath>

// ============================================================
// GLSL shaders — textured terrain with lighting
// ============================================================
static const char* kTerrainVert = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_color;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in float a_tex0;
layout(location = 4) in float a_tex1;
layout(location = 5) in float a_blend;
layout(location = 6) in vec2 a_tile_coord;
uniform mat4 u_mvp;
out vec3 v_color;
out vec3 v_normal;
out vec3 v_world_pos;
flat out float v_tex0;
flat out float v_tex1;
out float v_blend;
out vec2 v_tile_coord;
void main() {
    v_color = a_color;
    v_normal = a_normal;
    v_world_pos = a_pos;
    v_tex0 = a_tex0;
    v_tex1 = a_tex1;
    v_blend = a_blend;
    v_tile_coord = a_tile_coord;
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}
)";

static const char* kTerrainFrag = R"(
#version 330 core
in vec3 v_color;
in vec3 v_normal;
in vec3 v_world_pos;
flat in float v_tex0;
flat in float v_tex1;
in float v_blend;
in vec2 v_tile_coord;
uniform vec3 u_light_dir;
uniform float u_lighting;
uniform sampler2DArray u_tex_array;
uniform float u_use_tex;
out vec4 frag_color;
void main() {
    vec3 base_color = v_color;
    if (u_use_tex > 0.5) {
        vec2 uv = v_tile_coord;
        vec3 c0 = texture(u_tex_array, vec3(uv, v_tex0)).rgb;
        vec3 c1 = texture(u_tex_array, vec3(uv, v_tex1)).rgb;
        base_color = mix(c0, c1, v_blend);
    }
    vec3 N = normalize(v_normal);
    float diff = max(dot(N, u_light_dir), 0.0);
    float ambient = 0.35;
    float lighting = ambient + (1.0 - ambient) * diff;
    vec3 c = mix(base_color, base_color * lighting, u_lighting);
    frag_color = vec4(c, 1.0);
}
)";

// ============================================================
// TerrainWidget implementation
// ============================================================
TerrainWidget::TerrainWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMouseTracking(true);
}

TerrainWidget::~TerrainWidget() {
    makeCurrent();
    if (program_)     glDeleteProgram(program_);
    if (vao_)         glDeleteVertexArrays(1, &vao_);
    if (vbo_)         glDeleteBuffers(1, &vbo_);
    if (ebo_)         glDeleteBuffers(1, &ebo_);
    if (tex_array_)   glDeleteTextures(1, &tex_array_);
    doneCurrent();
}

void TerrainWidget::load_terrain(const Terrain& terrain) {
    terrain_ = std::make_unique<Terrain>(terrain);
    has_terrain_ = true;
    tex_dirty_ = true;

    // Camera: center of the map in world space
    float cx = terrain_->center_offset_x + (terrain_->tile_width - 1) * kTileSize * 0.5f;
    float cz = terrain_->center_offset_y + (terrain_->tile_height - 1) * kTileSize * 0.5f;
    cam_center_ = QPointF(cx, cz);

    // Auto-fit camera distance
    float max_dim = static_cast<float>(qMax(terrain_->tile_width, terrain_->tile_height)) * kTileSize;
    cam_distance_ = max_dim * 0.8f;
    cam_distance_ = qBound(100.0f, cam_distance_, 10000.0f);

    mesh_dirty_ = true;
    update();
}

void TerrainWidget::clear_terrain() {
    has_terrain_ = false;
    terrain_.reset();
    vertex_count_ = 0;
    index_count_ = 0;
    update();
}

// ============================================================
// OpenGL initialization
// ============================================================
void TerrainWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Terrain shader
    GLuint vs = compile_shader(GL_VERTEX_SHADER, kTerrainVert);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kTerrainFrag);
    if (vs && fs) {
        program_ = link_program(vs, fs);
        if (program_) {
            u_mvp_       = glGetUniformLocation(program_, "u_mvp");
            u_light_dir_ = glGetUniformLocation(program_, "u_light_dir");
            u_lighting_  = glGetUniformLocation(program_, "u_lighting");
            u_use_tex_   = glGetUniformLocation(program_, "u_use_tex");
            u_tex_array_ = glGetUniformLocation(program_, "u_tex_array");
        }
    }
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
}

GLuint TerrainWidget::compile_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        qWarning() << "TerrainWidget: shader compile error:" << log;
        return 0;
    }
    return shader;
}

GLuint TerrainWidget::link_program(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        qWarning() << "TerrainWidget: program link error:" << log;
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// ============================================================
// Height color (fallback when textures are not available)
// ============================================================
QColor TerrainWidget::height_color(float h) const {
    float min_h = 0x1800;
    float max_h = 0x2800;
    if (terrain_ && !terrain_->tilepoints.empty()) {
        auto [min_it, max_it] = std::minmax_element(
            terrain_->tilepoints.begin(), terrain_->tilepoints.end(),
            [](const TilePoint& a, const TilePoint& b) { return a.height < b.height; });
        min_h = min_it->height;
        max_h = max_it->height;
        if (max_h - min_h < 1.0f) {
            min_h -= 50.0f;
            max_h += 50.0f;
        }
    }

    float t = (h - min_h) / (max_h - min_h);
    t = qBound(0.0f, t, 1.0f);

    if (t < 0.5f) {
        float s = t * 2.0f;
        return QColor::fromRgbF(0.2f + s * 0.4f, 0.4f + s * 0.2f, 0.1f + s * 0.1f);
    } else {
        float s = (t - 0.5f) * 2.0f;
        return QColor::fromRgbF(0.6f + s * 0.3f, 0.6f + s * 0.3f, 0.2f + s * 0.6f);
    }
}

// ============================================================
// Tile color from tile ID suffix
// ============================================================
static QColor tile_color_from_id(const QString& tileId) {
    if (tileId.length() >= 4) {
        QString type = tileId.mid(1, 3);
        if (type == "grs" || type == "pla")  return {70, 150, 50};
        if (type == "grh")                   return {60, 130, 40};
        if (type == "drt")                   return {160, 130, 85};
        if (type == "dro" || type == "roc")  return {145, 115, 70};
        if (type == "rok")                   return {120, 115, 105};
        if (type == "sor" || type == "sqd")  return {155, 145, 130};
        if (type == "snw" || type == "hwa")  return {200, 210, 220};
        if (type == "dsr")                   return {205, 175, 110};
        if (type == "blp")                   return {55, 50, 45};
        if (type == "bbb")                   return {130, 100, 70};
        if (type == "clf")                   return {105, 95, 72};
        if (type == "wtf" || type == "gtf")  return {145, 140, 130};
        if (type == "hdg")                   return {50, 95, 35};
        if (type == "dos")                   return {55, 100, 35};
    }
    return {130, 120, 100};
}

// ============================================================
// Procedural texture generation (fallback)
// ============================================================
QImage TerrainWidget::generate_noise_texture(int w, int h,
                                             const QColor& base,
                                             const QColor& var,
                                             float scale)
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
        int iy = (int)ny;
        float fy = ny - iy;
        for (int x = 0; x < w; ++x) {
            float nx = (x / (float)w) * scale;
            int ix = (int)nx;
            float fx = nx - ix;
            float n00 = hash(ix, iy);
            float n10 = hash(ix + 1, iy);
            float n01 = hash(ix, iy + 1);
            float n11 = hash(ix + 1, iy + 1);
            float n = (n00 * (1 - fx) + n10 * fx) * (1 - fy)
                    + (n01 * (1 - fx) + n11 * fx) * fy;
            int r = qBound(0, (int)(base.red()   + var.red()   * (n - 0.5f) * 2.0f), 255);
            int g = qBound(0, (int)(base.green() + var.green() * (n - 0.5f) * 2.0f), 255);
            int b = qBound(0, (int)(base.blue()  + var.blue()  * (n - 0.5f) * 2.0f), 255);
            line[x * 3]     = (uchar)r;
            line[x * 3 + 1] = (uchar)g;
            line[x * 3 + 2] = (uchar)b;
        }
    }
    return img;
}

QImage TerrainWidget::generate_tile_texture(const QString& tileId, int idx) {
    QColor base, var;
    float scale = 6.0f;

    auto set = [&](int r, int g, int b, int vr, int vg, int vb) {
        base = QColor(r, g, b);
        var  = QColor(vr, vg, vb);
    };

    if (tileId.length() >= 4) {
        QString type = tileId.mid(1, 3);
        if (type == "grs" || type == "pla")       set( 70, 145,  50, 40, 50, 25);
        else if (type == "grh")                   set( 55, 125,  40, 35, 45, 20);
        else if (type == "drt")                   set(155, 130,  85, 40, 35, 30);
        else if (type == "dro" || type == "roc")  set(140, 115,  70, 35, 30, 25);
        else if (type == "rok")                   set(120, 115, 105, 30, 25, 25);
        else if (type == "sor" || type == "sqd")  set(150, 140, 130, 30, 30, 30);
        else if (type == "snw" || type == "hwa")  set(195, 205, 215, 30, 25, 20);
        else if (type == "dsr")                   set(200, 175, 110, 35, 35, 30);
        else if (type == "blp")                   set( 55,  50,  45, 15, 15, 15);
        else if (type == "bbb")                   set(130, 100,  70, 30, 25, 20);
        else if (type == "clf")                   set(105,  95,  72, 25, 20, 20);
        else if (type == "wtf" || type == "gtf")  set(150, 145, 135, 25, 25, 25);
        else if (type == "hdg")                   set( 50,  95,  35, 30, 35, 20);
        else if (type == "dos")                   set( 50,  95,  30, 30, 35, 20);
        else                                       set(130, 120, 100, 40, 40, 40);
    } else {
        set(130, 120, 100, 40, 40, 40);
    }

    return generate_noise_texture(tile_size_, tile_size_, base, var, scale);
}

// ============================================================
// Texture loading from WC3 MPQ
// ============================================================
// Try to load file from local Data/ directory (relative to exe or CWD)
static std::vector<uint8_t> read_local_slk(const char* filename) {
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/../../Data/" + filename,
        QCoreApplication::applicationDirPath() + "/../Data/" + filename,
        QCoreApplication::applicationDirPath() + "/Data/" + filename,
        QDir::currentPath() + "/Data/" + filename,
    };
    for (auto& path : candidates) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            auto bytes = f.readAll();
            f.close();
            qWarning().noquote() << "[TerrainWidget] Loaded" << filename << "from" << path;
            return {bytes.begin(), bytes.end()};
        }
    }
    return {};
}

// Load tile → path mapping from Terrain.slk in local Data/ or WC3 MPQ.
// Cache the map globally so we only parse once.
static QHash<QString, QString> load_tile_path_map(MapBuilder* builder) {
    static QHash<QString, QString> s_cache;
    static bool s_tried = false;
    if (s_tried) return s_cache;
    s_tried = true;

    std::vector<uint8_t> data = read_local_slk("Terrain.slk");
    if (data.empty() && builder) {
        // Try both backslash and forward slash paths
        data = builder->read_resource("TerrainArt\\Terrain.slk");
        if (data.empty())
            data = builder->read_resource("TerrainArt/Terrain.slk");
    }
    if (data.empty())
        return s_cache;

    std::string text(data.begin(), data.end());
    auto table = slk::parse(text);

    for (size_t r = 0; r < table.rows.size(); ++r) {
        auto& row = table.rows[r];
        if (row.size() < 4) continue;
        auto& tileId = row[0];
        auto& dir    = row[2];
        auto& file   = row[3];
        if (tileId.empty() || dir.empty() || file.empty()) continue;
        QString path = QString::fromStdString(dir + "\\" + file + ".blp");
        s_cache.insert(QString::fromStdString(tileId), path);
    }
    return s_cache;
}

void TerrainWidget::upload_textures() {
    if (!terrain_ || terrain_->ground_tiles.empty()) {
        tex_count_ = 0;
        return;
    }

    if (!isValid()) return;

    if (tex_array_) {
        glDeleteTextures(1, &tex_array_);
        tex_array_ = 0;
    }

    tex_count_ = (int)terrain_->ground_tiles.size();
    if (tex_count_ <= 0) return;

    constexpr int kVariantsPerTile = 32;
    constexpr int kGridCols = 8;
    constexpr int kGridRows = 4;

    std::vector<QImage> all_variants;
    all_variants.reserve(tex_count_ * kVariantsPerTile);
    layer_offsets_.resize(tex_count_);
    tex_extended_.resize(tex_count_);
    tile_size_ = 64;

    auto pathMap = load_tile_path_map(builder_);
    int loaded_from_blp = 0;

    for (int i = 0; i < tex_count_; ++i) {
        layer_offsets_[i] = i * kVariantsPerTile;
        bool got_blp = false;

        QString tileId = QString::fromStdString(terrain_->ground_tiles[i]);
        QString mpqPath;
        auto it = pathMap.find(tileId);
        if (it != pathMap.end()) mpqPath = it.value();

        if (builder_ && !mpqPath.isEmpty()) {
            std::vector<uint8_t> raw = builder_->read_raw(mpqPath.toStdString());
            if (raw.empty())
                raw = builder_->read_resource(mpqPath.toStdString());
            if (!raw.empty()) {
                QImage blp = read_blp(raw).convertToFormat(QImage::Format_RGB888);
                if (!blp.isNull()) {
                    ++loaded_from_blp;
                    got_blp = true;

                    int w = blp.width(), h = blp.height();
                    int sub_size = std::max(h / kGridRows, 1);
                    int cols = std::min(w / sub_size, kGridCols);
                    tile_size_ = std::max(tile_size_, sub_size);
                    tex_extended_[i] = (cols > 4);

                    for (int vy = 0; vy < kGridRows; ++vy) {
                        for (int vx = 0; vx < cols; ++vx) {
                            all_variants.push_back(
                                blp.copy(vx * sub_size, vy * sub_size, sub_size, sub_size)
                                    .convertToFormat(QImage::Format_RGB888));
                        }
                    }
                    int have = (int)all_variants.size() - layer_offsets_[i];
                    for (int j = have; j < kVariantsPerTile; ++j)
                        all_variants.push_back(all_variants[all_variants.size() - have]);
                }
            }
        }

        if (!got_blp) {
            tex_extended_[i] = false;
            qWarning().noquote() << "[TerrainWidget] BLP fallback for" << tileId << "path=" << mpqPath;
            QImage proc = generate_tile_texture(tileId, i)
                .scaled(tile_size_, tile_size_, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                .convertToFormat(QImage::Format_RGB888);
            for (int j = 0; j < kVariantsPerTile; ++j)
                all_variants.push_back(proc);
        }
    }

    for (auto& img : all_variants) {
        if (img.width() != tile_size_ || img.height() != tile_size_)
            img = img.scaled(tile_size_, tile_size_, Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation);
    }

    int total_layers = tex_count_ * kVariantsPerTile;

    qWarning().noquote()
        << QStringLiteral("[TerrainWidget] Textures: %1/%2 from BLP, %3 layers, %4px")
               .arg(loaded_from_blp).arg(tex_count_)
               .arg(total_layers).arg(tile_size_);

    glGenTextures(1, &tex_array_);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex_array_);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8,
                 tile_size_, tile_size_, total_layers,
                 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int layer = 0; layer < total_layers; ++layer) {
        if (all_variants[layer].isNull()) continue;
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer,
                        tile_size_, tile_size_, 1,
                        GL_RGB, GL_UNSIGNED_BYTE, all_variants[layer].bits());
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void TerrainWidget::free_textures() {
    if (!isValid()) return;
    if (tex_array_) {
        glDeleteTextures(1, &tex_array_);
        tex_array_ = 0;
        tex_count_ = 0;
    }
}

// ============================================================
// Mesh building with normals and texture indices
// ============================================================
void TerrainWidget::build_mesh() {
    if (!terrain_) return;

    int cols = terrain_->tile_width;
    int rows = terrain_->tile_height;

    struct Vertex { float x, y, z, r, g, b, nx, ny, nz, tex0, tex1, blend, tx, tz; };

    int quads_x = cols - 1;
    int quads_z = rows - 1;
    int num_quads = quads_x * quads_z;

    std::vector<Vertex> vertices;
    vertices.reserve(num_quads * 4);

    std::vector<GLuint> indices;
    indices.reserve(num_quads * 6);

    auto get_h = [&](int col, int row) -> float {
        return terrain_->tilepoints[row * cols + col].height;
    };

    for (int qrow = 0; qrow < quads_z; ++qrow) {
        for (int qcol = 0; qcol < quads_x; ++qcol) {
            int cc[4] = {qcol, qcol+1, qcol, qcol+1};
            int cr[4] = {qrow, qrow, qrow+1, qrow+1};

            int tex_count[16] = {};
            uint8_t corner_tex[4];
            for (int c = 0; c < 4; ++c) {
                uint8_t t = terrain_->tilepoints[cr[c] * cols + cc[c]].ground_texture;
                corner_tex[c] = t;
                if (t < 16) tex_count[t]++;
            }

            int primary = corner_tex[0], secondary = corner_tex[0];
            int pc = 0, sc = 0;
            for (int t = 0; t < 16; ++t) {
                if (tex_count[t] > pc) {
                    sc = pc; secondary = primary;
                    pc = tex_count[t]; primary = t;
                } else if (tex_count[t] > sc) {
                    sc = tex_count[t]; secondary = t;
                }
            }
            if (sc == 0) secondary = primary;

            // 1-2-4-8 corner flag system for transition masks (WC3 standard):
            // BL=1, BR=2, TL=4, TR=8. Sum the flags of corners that have each texture.
            const int kQuadFlags[4] = {1, 2, 4, 8};
            auto calc_variant = [&](int tex) -> int {
                if (tex < 0 || tex >= (int)tex_extended_.size()) return 0;
                int sum = 0;
                for (int c = 0; c < 4; ++c)
                    if (corner_tex[c] == (uint8_t)tex) sum += kQuadFlags[c];
                if (sum == 15) {
                    if (tex_extended_[tex]) {
                        int var = 0;
                        for (int c = 0; c < 4; ++c) {
                            if (corner_tex[c] == (uint8_t)tex) {
                                int idx = cr[c] * cols + cc[c];
                                var = terrain_->tilepoints[idx].texture_detail & 0x1F;
                                break;
                            }
                        }
                        return 16 + (var & 15);
                    }
                    return 0;
                }
                return sum;
            };
            int v0 = calc_variant(primary);
            int v1 = (secondary == primary) ? v0 : calc_variant(secondary);
            int layer0 = layer_offsets_[primary]   + v0;
            int layer1 = layer_offsets_[secondary] + v1;

            int base = (int)vertices.size();

            for (int c = 0; c < 4; ++c) {
                int col = cc[c], row = cr[c];
                const auto& tp = terrain_->tilepoints[row * cols + col];

                float wx = terrain_->center_offset_x + col * kTileSize;
                float wz = terrain_->center_offset_y + row * kTileSize;
                float wy = tp.height;

                QColor cv = show_texture_ ? Qt::white : height_color(tp.height);

                float hR = (col < cols - 1) ? get_h(col + 1, row) : tp.height;
                float hL = (col > 0)        ? get_h(col - 1, row) : tp.height;
                float hU = (row < rows - 1) ? get_h(col, row + 1) : tp.height;
                float hD = (row > 0)        ? get_h(col, row - 1) : tp.height;
                float dx = (hR - hL) / (2.0f * kTileSize);
                float dz = (hU - hD) / (2.0f * kTileSize);
                float len = std::sqrt(dx * dx + dz * dz + 1.0f);

                float blend = (corner_tex[c] == (uint8_t)secondary) ? 1.0f : 0.0f;

                vertices.push_back({
                    wx, wy, wz,
                    (float)cv.redF(), (float)cv.greenF(), (float)cv.blueF(),
                    -dx / len, 1.0f / len, -dz / len,
                    (float)layer0, (float)layer1, blend,
                    (float)(c & 1), (float)(c >> 1)
                });
            }

            indices.push_back(base);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 1);
            indices.push_back(base + 3);
            indices.push_back(base + 2);
        }
    }

    vertex_count_ = (int)vertices.size();
    index_count_ = (int)indices.size();

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                 vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
                 indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tex0));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tex1));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, blend));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tx));
    glEnableVertexAttribArray(6);

    glBindVertexArray(0);
}

void TerrainWidget::upload_mesh() {
    if (!isValid()) return;

    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    vao_ = vbo_ = ebo_ = 0;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    if (has_terrain_)
        build_mesh();
}

// ============================================================
// Rendering
// ============================================================
void TerrainWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!has_terrain_ || !program_ || vertex_count_ == 0)
        return;

    // Lazy texture upload
    if (tex_dirty_) {
        if (tex_array_)
            free_textures();
        upload_textures();
        tex_dirty_ = false;
    }

    // Lazy mesh upload
    if (mesh_dirty_) {
        upload_mesh();
        mesh_dirty_ = false;
    }

    // Camera matrices
    float aspect = static_cast<float>(width()) / static_cast<float>(qMax(height(), 1));
    float yaw   = qDegreesToRadians(cam_yaw_);
    float pitch = qDegreesToRadians(cam_pitch_);
    float cosP  = qCos(pitch);
    float sinP  = qAbs(qSin(pitch));

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

    QMatrix4x4 mvp = proj * view;

    // Draw terrain
    glUseProgram(program_);
    glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, mvp.constData());
    glUniform3f(u_light_dir_, 0.5f, 0.8f, 0.3f);
    glUniform1f(u_lighting_, 1.0f); // always lit

    bool useTex = tex_array_ > 0 && tex_count_ > 0;
    glUniform1f(u_use_tex_, useTex ? 1.0f : 0.0f);
    if (tex_array_) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex_array_);
        glUniform1i(u_tex_array_, 0);
    }

    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glUseProgram(0);
}

void TerrainWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

// ============================================================
// Camera controls
// ============================================================
void TerrainWidget::mousePressEvent(QMouseEvent* event) {
    if (!has_terrain_) return;
    dragging_ = true;
    drag_button_ = event->button();
    last_mouse_pos_ = event->position();
}

void TerrainWidget::mouseMoveEvent(QMouseEvent* event) {
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
        forward.normalize();
        right.normalize();
        cam_center_ += QPointF(right.x() * dx * scale, right.z() * dx * scale);
        cam_center_ -= QPointF(forward.x() * dy * scale, forward.z() * dy * scale);
        update();
    }
}

void TerrainWidget::mouseReleaseEvent(QMouseEvent* /*event*/) {
    dragging_ = false;
}

void TerrainWidget::wheelEvent(QWheelEvent* event) {
    if (!has_terrain_) return;
    float delta = static_cast<float>(event->angleDelta().y()) / 120.0f;
    cam_distance_ *= (1.0f - delta * 0.1f);
    cam_distance_ = qBound(10.0f, cam_distance_, 100000.0f);
    update();
}

// ============================================================
// TerrainEditorPlugin
// ============================================================
bool TerrainEditorPlugin::init(PluginContext& ctx) {
    builder_ = ctx.builder;
    widget_ = new TerrainWidget(ctx.parent_widget);
    widget_->set_builder(builder_);
    widget_->set_wc3_manager(ctx.wc3);
    return true;
}

void TerrainEditorPlugin::activate() {
    if (!widget_ || !builder_) return;
    try {
        auto terrain = builder_->read_terrain();
        widget_->load_terrain(terrain);
    } catch (...) {
        widget_->clear_terrain();
    }
}

void TerrainEditorPlugin::deactivate() {
    if (widget_) widget_->clear_terrain();
}

void TerrainEditorPlugin::destroy() {
    delete widget_;
    widget_ = nullptr;
    builder_ = nullptr;
}
