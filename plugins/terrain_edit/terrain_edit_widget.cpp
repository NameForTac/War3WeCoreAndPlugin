#include "terrain_edit_widget.h"
#include "blp_reader.h"
#include <QOpenGLContext>
#include <QPainter>
#include <QDebug>
#include <QtMath>
#include <QApplication>
#include <algorithm>
#include <cmath>

// ============================================================
// GLSL shaders
// ============================================================
static const char* kTerrainVert = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_color;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in float a_tex_index;
uniform mat4 u_mvp;
out vec3 v_color;
out vec3 v_normal;
out vec3 v_world_pos;
out float v_tex_index;
void main() {
    v_color = a_color;
    v_normal = a_normal;
    v_world_pos = a_pos;
    v_tex_index = a_tex_index;
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}
)";

static const char* kTerrainFrag = R"(
#version 330 core
in vec3 v_color;
in vec3 v_normal;
in vec3 v_world_pos;
in float v_tex_index;
uniform vec3 u_light_dir;
uniform float u_lighting;
uniform sampler2DArray u_tex_array;
uniform float u_use_tex;
uniform float u_tex_tile;
out vec4 frag_color;
void main() {
    vec3 base_color = v_color;
    if (u_use_tex > 0.5) {
        vec2 uv = fract(v_world_pos.xz / u_tex_tile);
        float layer = floor(v_tex_index + 0.5);
        base_color = texture(u_tex_array, vec3(uv, layer)).rgb;
    }
    vec3 N = normalize(v_normal);
    float diff = max(dot(N, u_light_dir), 0.0);
    float ambient = 0.35;
    float lighting = ambient + (1.0 - ambient) * diff;
    vec3 c = mix(base_color, base_color * lighting, u_lighting);
    frag_color = vec4(c, 1.0);
}
)";

static const char* kGridVert = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
uniform mat4 u_mvp;
void main() {
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}
)";

static const char* kGridFrag = R"(
#version 330 core
uniform vec4 u_color;
out vec4 frag_color;
void main() {
    frag_color = u_color;
}
)";

// ============================================================
// Construction
// ============================================================
TerrainEditWidget::TerrainEditWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMouseTracking(true);
}

TerrainEditWidget::~TerrainEditWidget() {
    makeCurrent();
    if (program_)      glDeleteProgram(program_);
    if (grid_program_) glDeleteProgram(grid_program_);
    if (vao_)          glDeleteVertexArrays(1, &vao_);
    if (vbo_)          glDeleteBuffers(1, &vbo_);
    if (ebo_)          glDeleteBuffers(1, &ebo_);
    if (grid_vao_)     glDeleteVertexArrays(1, &grid_vao_);
    if (grid_vbo_)     glDeleteBuffers(1, &grid_vbo_);
    if (tex_array_)    glDeleteTextures(1, &tex_array_);
    doneCurrent();
}

namespace { int g_last_grid_step = 1; }

// ============================================================
// Load terrain
// ============================================================
void TerrainEditWidget::loadTerrain(Terrain* terrain) {
    terrain_ = terrain;
    has_terrain_ = (terrain != nullptr);

    qDebug() << "[TerrainEdit] loadTerrain has_terrain_=" << has_terrain_;

    if (has_terrain_) {
        qDebug() << "[TerrainEdit]  tile_width=" << terrain_->tile_width
                 << "tile_height=" << terrain_->tile_height
                 << "tilepoints=" << terrain_->tilepoints.size()
                 << "ground_tiles=" << terrain_->ground_tiles.size()
                 << "center_offset_x=" << terrain_->center_offset_x
                 << "center_offset_y=" << terrain_->center_offset_y;
        tex_dirty_ = true;

        float cx = terrain_->center_offset_x + (terrain_->tile_width - 1) * kTileSize * 0.5f;
        float cz = terrain_->center_offset_y + (terrain_->tile_height - 1) * kTileSize * 0.5f;
        cam_center_ = QPointF(cx, cz);

        // Reset camera to default angle, facing terrain center
        cam_yaw_ = -45.0f;
        cam_pitch_ = -35.0f;

        // Auto-fit distance: terrain should fill ~75% of the viewport
        float max_dim = static_cast<float>(
            qMax(terrain_->tile_width, terrain_->tile_height)) * kTileSize;
        float pitch_rad = qDegreesToRadians(qAbs(cam_pitch_));
        float cosP = qCos(pitch_rad);
        float needed_dist = (max_dim * 0.5f) / (cosP * 0.375f);  // tan(FOV/2) ≈ 0.414
        cam_distance_ = qBound(100.0f, needed_dist, 100000.0f);
    }

    mesh_dirty_ = true;
    g_last_grid_step = 1;
    hover_col_ = -1;
    hover_row_ = -1;
    update();
}

// ============================================================
// OpenGL helpers
// ============================================================
GLuint TerrainEditWidget::compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        qWarning("TerrainEdit: shader compile error: %s", log);
        return 0;
    }
    return shader;
}

GLuint TerrainEditWidget::linkProgram(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        qWarning("TerrainEdit: program link error: %s", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// ============================================================
// Color helpers
// ============================================================
QColor TerrainEditWidget::heightColor(float h) const {
    if (!terrain_ || terrain_->tilepoints.empty()) {
        return QColor::fromRgbF(0.4f, 0.6f, 0.3f);
    }

    auto [min_it, max_it] = std::minmax_element(
        terrain_->tilepoints.begin(), terrain_->tilepoints.end(),
        [](const TilePoint& a, const TilePoint& b) { return a.height < b.height; });
    float min_h = min_it->height;
    float max_h = max_it->height;
    if (max_h - min_h < 1.0f) {
        min_h -= 50.0f;
        max_h += 50.0f;
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

QColor TerrainEditWidget::textureColor(int tex_idx) const {
    static const QColor colors[] = {
        QColor(120, 140, 100), QColor(100, 180, 80),  QColor(180, 160, 100),
        QColor(160, 120, 80),  QColor(200, 180, 130), QColor(140, 200, 120),
        QColor(130, 110, 80),  QColor(210, 200, 150), QColor(100, 100, 120),
        QColor(160, 150, 140), QColor(80, 100, 60),   QColor(200, 180, 110),
        QColor(140, 120, 100), QColor(190, 190, 210), QColor(100, 140, 180),
        QColor(70, 70, 70),
    };
    if (tex_idx >= 0 && tex_idx < 16)
        return colors[tex_idx];
    return colors[0];
}

QColor TerrainEditWidget::tileColor(const QString& tileId, int fallback_idx) const {
    // Map tile ID suffix (3-char type) to natural-looking colors
    if (tileId.length() >= 4) {
        QString type = tileId.mid(1, 3);  // skip tileset letter, e.g. "Ldrt" → "drt"
        // Grass types
        if (type == "grs") return {70, 150, 50};
        if (type == "grh") return {60, 130, 40};
        if (type == "pla") return {90, 170, 60};
        // Dirt types
        if (type == "drt") return {160, 130, 85};
        if (type == "dro") return {145, 115, 70};
        if (type == "roc") return {155, 125, 80};
        // Rock / stone
        if (type == "rok") return {120, 115, 105};
        if (type == "sor") return {155, 145, 130};
        if (type == "sqd") return {140, 135, 125};
        if (type == "blp") return {55, 50, 45};
        if (type == "bbb") return {130, 100, 70};
        if (type == "clf") return {105, 95, 72};
        // Urban
        if (type == "wtf") return {145, 140, 130};
        if (type == "gtf") return {155, 150, 140};
        if (type == "hdg") return {50, 95, 35};
        // Snow
        if (type == "snw") return {200, 210, 220};
        if (type == "hwa") return {185, 195, 205};
        // Desert / sand
        if (type == "dsr") return {205, 175, 110};
        // Dense forest / undergrowth
        if (type == "dos") return {55, 100, 35};
    }
    // Fallback to the indexed palette
    return textureColor(fallback_idx);
}

// ============================================================
// Procedural texture generation
// ============================================================
QImage TerrainEditWidget::generateNoiseTexture(int w, int h,
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

QImage TerrainEditWidget::generateTileTexture(const QString& tileId, int idx) {
    // Pick base color and variation from tile type suffix
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
        // Fallback: map from textureColor palette
        QColor c = textureColor(idx);
        set(c.red(), c.green(), c.blue(), 40, 40, 40);
    }

    return generateNoiseTexture(kTexSize_, kTexSize_, base, var, scale);
}

// ============================================================
// Tile ID → BLP path in MPQ, and procedural texture generation
// ============================================================

// Map the first character of a tile ID (e.g. "Ldrt" → 'L') to
// the tileset folder name inside War3.mpq's TerrainArt\ directory.
static QString tilesetFolder(QChar ts) {
    switch (ts.toUpper().unicode()) {
    case 'A': return "Ashenvale";
    case 'B': return "Barrens";
    case 'C': return "Felwood";
    case 'D': return "Dungeon";
    case 'F': return "LordaeronFall";
    case 'G': return "Underground";
    case 'I': return "Icecrown";
    case 'J': return "DalaranRuins";
    case 'K': return "BlackCitadel";
    case 'L': return "LordaeronSummer";
    case 'N': return "Northrend";
    case 'O': return "Outland";
    case 'Q': return "VillageFall";
    case 'V': return "Village";
    case 'W': return "LordaeronWinter";
    case 'X': return "Dalaran";
    case 'Y': return "Cityscape";
    case 'Z': return "SunkenRuins";
    default:  return {};
    }
}

// Convert a tile ID like "Ldrt" to the MPQ path like "TerrainArt\LordaeronSummer\Ldrt.blp"
static QString tileIdToBlpPath(const QString& tileId) {
    if (tileId.length() < 1) return {};
    QString folder = tilesetFolder(tileId[0]);
    if (folder.isEmpty()) return {};
    return QString("TerrainArt\\%1\\%2.blp").arg(folder).arg(tileId);
}

void TerrainEditWidget::uploadTextures() {
    if (!terrain_ || terrain_->ground_tiles.empty()) {
        qDebug() << "[TerrainEdit] uploadTextures: no tiles to upload";
        tex_count_ = 0;
        return;
    }

    if (!isValid()) {
        qWarning() << "[TerrainEdit] uploadTextures: widget not valid!";
        return;
    }

    if (tex_array_) {
        glDeleteTextures(1, &tex_array_);
        tex_array_ = 0;
    }

    tex_count_ = (int)terrain_->ground_tiles.size();
    qDebug() << "[TerrainEdit] uploadTextures: tex_count_=" << tex_count_;
    if (tex_count_ <= 0) {
        qWarning() << "[TerrainEdit] uploadTextures: tex_count_ <= 0!";
        return;
    }

    // Pre-load all tile textures (try BLP from MPQ, fallback to procedural)
    int tex_w = 128;  // use 128x128 for sharper terrain textures
    int tex_h = 128;
    std::vector<QImage> tile_images(tex_count_);

    for (int i = 0; i < tex_count_; ++i) {
        QString tileId = QString::fromStdString(terrain_->ground_tiles[i]);

        // Try loading the real BLP texture from WC3 MPQ
        if (!wc3_data_dir_.empty()) {
            QString mpqPath = tileIdToBlpPath(tileId);
            if (!mpqPath.isEmpty()) {
                QImage blpImg = load_wc3_texture(wc3_data_dir_, mpqPath.toStdString());
                if (!blpImg.isNull()) {
                    // Scale to our target size
                    tile_images[i] = blpImg.scaled(tex_w, tex_h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                            .convertToFormat(QImage::Format_RGB888);
                    qDebug() << "[TerrainEdit] loaded BLP texture for" << tileId << mpqPath;
                    continue;
                } else {
                    qDebug() << "[TerrainEdit] BLP fallback for" << tileId << mpqPath;
                }
            }
        }

        // Fallback: generate procedural texture
        QImage img = generateTileTexture(tileId, i);
        if (img.isNull()) {
            qWarning() << "[TerrainEdit] generated null texture for" << tileId;
            // Create a magenta placeholder so we know something is wrong
            img = QImage(tex_w, tex_h, QImage::Format_RGB888);
            img.fill(QColor(255, 0, 255));
        }
        tile_images[i] = img;
    }

    glGenTextures(1, &tex_array_);
    qDebug() << "[TerrainEdit] uploadTextures: tex_array_=" << tex_array_;

    glBindTexture(GL_TEXTURE_2D_ARRAY, tex_array_);
    GLenum texErr = glGetError();
    if (texErr != GL_NO_ERROR)
        qWarning() << "[TerrainEdit] glBindTexture error:" << texErr;

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, tex_w, tex_h, tex_count_,
                 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    texErr = glGetError();
    if (texErr != GL_NO_ERROR)
        qWarning() << "[TerrainEdit] glTexImage3D error:" << texErr;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int i = 0; i < tex_count_; ++i) {
        if (tile_images[i].isNull()) continue;
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i,
                        tex_w, tex_h, 1, GL_RGB, GL_UNSIGNED_BYTE, tile_images[i].bits());
        texErr = glGetError();
        if (texErr != GL_NO_ERROR)
            qWarning() << "[TerrainEdit] glTexSubImage3D layer" << i << "error:" << texErr;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void TerrainEditWidget::freeTextures() {
    if (!isValid()) return;
    if (tex_array_) {
        glDeleteTextures(1, &tex_array_);
        tex_array_ = 0;
        tex_count_ = 0;
    }
}

// ============================================================
// OpenGL initialization
// ============================================================
void TerrainEditWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Terrain shader
    GLuint vs = compileShader(GL_VERTEX_SHADER, kTerrainVert);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kTerrainFrag);
    qDebug() << "[TerrainEdit] Terrain vs=" << vs << "fs=" << fs;
    if (vs && fs) {
        program_ = linkProgram(vs, fs);
        qDebug() << "[TerrainEdit] Terrain program_=" << program_;
        if (program_) {
            u_mvp_ = glGetUniformLocation(program_, "u_mvp");
            u_light_dir_ = glGetUniformLocation(program_, "u_light_dir");
            u_lighting_ = glGetUniformLocation(program_, "u_lighting");
            u_use_tex_ = glGetUniformLocation(program_, "u_use_tex");
            u_tex_array_ = glGetUniformLocation(program_, "u_tex_array");
            qDebug() << "[TerrainEdit] Uniforms:" << u_mvp_ << u_light_dir_ << u_lighting_ << u_use_tex_ << u_tex_array_;
        }
    } else {
        qWarning() << "[TerrainEdit] Shader compilation failed!";
    }
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);

    // Grid shader
    vs = compileShader(GL_VERTEX_SHADER, kGridVert);
    fs = compileShader(GL_FRAGMENT_SHADER, kGridFrag);
    if (vs && fs) {
        grid_program_ = linkProgram(vs, fs);
        if (grid_program_) {
            u_grid_mvp_ = glGetUniformLocation(grid_program_, "u_mvp");
        }
    }
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
}

// ============================================================
// Mesh building
// ============================================================
void TerrainEditWidget::buildMesh() {
    if (!terrain_) {
        qWarning() << "[TerrainEdit] buildMesh: terrain_ is null!";
        return;
    }

    int cols = terrain_->tile_width;
    int rows = terrain_->tile_height;
    qDebug() << "[TerrainEdit] buildMesh cols=" << cols << "rows=" << rows;

    struct Vertex { float x, y, z, r, g, b, nx, ny, nz, tex_index; };
    std::vector<Vertex> vertices;
    vertices.reserve(cols * rows);

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            int idx = row * cols + col;
            const auto& tp = terrain_->tilepoints[idx];

            float wx = terrain_->center_offset_x + col * kTileSize;
            float wz = terrain_->center_offset_y + row * kTileSize;
            float wy = tp.height;

            QColor c;
            if (show_texture_ && !terrain_->ground_tiles.empty()) {
                int tex_idx = tp.ground_texture;
                QString tileId;
                if (tex_idx >= 0 && tex_idx < (int)terrain_->ground_tiles.size())
                    tileId = QString::fromStdString(terrain_->ground_tiles[tex_idx]);
                c = tileColor(tileId, tex_idx);
            } else if (show_texture_) {
                c = textureColor(tp.ground_texture);
            } else {
                c = heightColor(tp.height);
            }

            // Compute normal from neighbor heights
            float hR = (col < cols - 1) ? terrain_->tilepoints[row * cols + (col + 1)].height : tp.height;
            float hL = (col > 0)        ? terrain_->tilepoints[row * cols + (col - 1)].height : tp.height;
            float hU = (row < rows - 1) ? terrain_->tilepoints[(row + 1) * cols + col].height : tp.height;
            float hD = (row > 0)        ? terrain_->tilepoints[(row - 1) * cols + col].height : tp.height;

            float dx = (hR - hL) / (2.0f * kTileSize);
            float dz = (hU - hD) / (2.0f * kTileSize);
            float len = std::sqrt(dx * dx + dz * dz + 1.0f);
            float nx = -dx / len;
            float ny =  1.0f / len;
            float nz = -dz / len;

            vertices.push_back({wx, wy, wz,
                                (float)c.redF(), (float)c.greenF(), (float)c.blueF(),
                                nx, ny, nz,
                                (float)tp.ground_texture});
        }
    }

    int quads_x = cols - 1;
    int quads_z = rows - 1;
    std::vector<GLuint> indices;
    indices.reserve(quads_x * quads_z * 6);

    for (int row = 0; row < quads_z; ++row) {
        for (int col = 0; col < quads_x; ++col) {
            int bl = row * cols + col;
            int br = bl + 1;
            int tl = (row + 1) * cols + col;
            int tr = tl + 1;
            indices.push_back(bl);
            indices.push_back(br);
            indices.push_back(tl);
            indices.push_back(br);
            indices.push_back(tr);
            indices.push_back(tl);
        }
    }

    vertex_count_ = (int)vertices.size();
    index_count_  = (int)indices.size();

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
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tex_index));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
}

void TerrainEditWidget::uploadMesh() {
    if (!isValid()) return;

    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    vao_ = vbo_ = ebo_ = 0;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    if (has_terrain_)
        buildMesh();
}

// ============================================================
// Grid with LOD: fewer lines when camera is far
// ============================================================
void TerrainEditWidget::uploadGrid() {
    if (!terrain_ || !isValid()) return;

    if (grid_vao_) glDeleteVertexArrays(1, &grid_vao_);
    if (grid_vbo_) glDeleteBuffers(1, &grid_vbo_);
    grid_vao_ = grid_vbo_ = 0;
    grid_vertex_count_ = 0;

    int cols = terrain_->tile_width;
    int rows = terrain_->tile_height;

    // Compute grid step based on camera distance (LOD)
    int step = 1;
    if (cam_distance_ > 10000.0f) step = 8;
    else if (cam_distance_ > 5000.0f) step = 4;
    else if (cam_distance_ > 2000.0f) step = 2;

    // Clamp step so we don't lose all lines on small maps
    int max_dim = qMax(cols, rows);
    while (step > 1 && max_dim / step < 6) step /= 2;

    std::vector<float> verts;

    // Horizontal lines
    for (int row = 0; row < rows; row += step) {
        for (int col = 0; col < cols - 1; ++col) {
            int idx0 = row * cols + col;
            int idx1 = row * cols + col + 1;
            float x0 = terrain_->center_offset_x + col * kTileSize;
            float z0 = terrain_->center_offset_y + row * kTileSize;
            float y0 = terrain_->tilepoints[idx0].height + 2.0f;
            float x1 = terrain_->center_offset_x + (col + 1) * kTileSize;
            float z1 = terrain_->center_offset_y + row * kTileSize;
            float y1 = terrain_->tilepoints[idx1].height + 2.0f;
            verts.insert(verts.end(), {x0, y0, z0, x1, y1, z1});
        }
    }

    // Vertical lines
    for (int col = 0; col < cols; col += step) {
        for (int row = 0; row < rows - 1; ++row) {
            int idx0 = row * cols + col;
            int idx1 = (row + 1) * cols + col;
            float x0 = terrain_->center_offset_x + col * kTileSize;
            float z0 = terrain_->center_offset_y + row * kTileSize;
            float y0 = terrain_->tilepoints[idx0].height + 2.0f;
            float x1 = terrain_->center_offset_x + col * kTileSize;
            float z1 = terrain_->center_offset_y + (row + 1) * kTileSize;
            float y1 = terrain_->tilepoints[idx1].height + 2.0f;
            verts.insert(verts.end(), {x0, y0, z0, x1, y1, z1});
        }
    }

    grid_vertex_count_ = (int)verts.size() / 3;

    glGenVertexArrays(1, &grid_vao_);
    glGenBuffers(1, &grid_vbo_);

    glBindVertexArray(grid_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// ============================================================
// Rendering
// ============================================================
void TerrainEditWidget::paintGL() {
    // Check for prior GL errors
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
        qWarning() << "[TerrainEdit] GL error BEFORE paintGL:" << err;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!has_terrain_ || !terrain_) {
        qDebug() << "[TerrainEdit] paintGL: no terrain (has_terrain_=" << has_terrain_ << "terrain_=" << terrain_ << ")";
        return;
    }

    // Debug: check program and vertex count
    GLint cur_prog = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &cur_prog);
    qDebug() << "[TerrainEdit] paintGL: program_=" << program_
             << "current_program=" << cur_prog
             << "vertex_count_=" << vertex_count_
             << "index_count_=" << index_count_
             << "render_mode_=" << (int)render_mode_
             << "show_texture_=" << show_texture_
             << "tex_array_=" << tex_array_
             << "cam_center=" << cam_center_
             << "cam_dist=" << cam_distance_
             << "cam_yaw=" << cam_yaw_
             << "cam_pitch=" << cam_pitch_;

    // Lazy texture upload (must happen in valid GL context)
    if (tex_dirty_) {
        if (tex_array_)
            freeTextures();
        uploadTextures();
        tex_dirty_ = false;
    }

    // Determine grid LOD step from current distance
    // (mesh_dirty_ will trigger uploadGrid which reads cam_distance_)
    if (mesh_dirty_) {
        uploadMesh();
        uploadGrid();
        mesh_dirty_ = false;
    }

    if (!program_ || vertex_count_ == 0) return;

    // Camera matrices
    float aspect = static_cast<float>(width()) / static_cast<float>(qMax(height(), 1));
    float yaw   = qDegreesToRadians(cam_yaw_);
    float pitch = qDegreesToRadians(cam_pitch_);
    float cosP  = qCos(pitch);
    float sinP  = qAbs(qSin(pitch));  // always above terrain

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

    qDebug() << "[TerrainEdit] Camera eye=" << eye << "center=" << center;

    // --- Draw terrain ---
    glUseProgram(program_);
    err = glGetError();
    if (err != GL_NO_ERROR) qWarning() << "[TerrainEdit] glUseProgram error:" << err;

    glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, mvp.constData());
    err = glGetError();
    if (err != GL_NO_ERROR) qWarning() << "[TerrainEdit] u_mvp error:" << err;

    // Light direction (fixed world-space, from upper-right-front)
    glUniform3f(u_light_dir_, 0.5f, 0.8f, 0.3f);
    err = glGetError();
    if (err != GL_NO_ERROR) qWarning() << "[TerrainEdit] u_light_dir error:" << err;

    // Texture setup (with bounds safety)
    bool useTex = show_texture_ && tex_array_ > 0 && tex_count_ > 0;
    glUniform1f(u_use_tex_, useTex ? 1.0f : 0.0f);
    glUniform1f(glGetUniformLocation(program_, "u_tex_tile"), kTileSize);
    if (tex_array_) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex_array_);
        glUniform1i(u_tex_array_, 0);
    }

    // Disable culling for debugging
    glDisable(GL_CULL_FACE);

    if (render_mode_ == RenderMode::Wireframe) {
        glUniform1f(u_lighting_, 0.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glBindVertexArray(vao_);
        err = glGetError();
        if (err != GL_NO_ERROR) qWarning() << "[TerrainEdit] before draw error:" << err;
        glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, nullptr);
        err = glGetError();
        if (err != GL_NO_ERROR) qWarning() << "[TerrainEdit] draw error:" << err;
        glBindVertexArray(0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    } else {
        glUniform1f(u_lighting_, (render_mode_ == RenderMode::Lit) ? 1.0f : 0.0f);
        glBindVertexArray(vao_);
        err = glGetError();
        if (err != GL_NO_ERROR) qWarning() << "[TerrainEdit] before draw error:" << err;
        glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, nullptr);
        err = glGetError();
        if (err != GL_NO_ERROR) qWarning() << "[TerrainEdit] draw error after:" << err;
        glBindVertexArray(0);
    }

    glEnable(GL_CULL_FACE);

    // --- Draw grid ---
    if (grid_program_ && grid_vertex_count_ > 0) {
        glUseProgram(grid_program_);
        glUniformMatrix4fv(u_grid_mvp_, 1, GL_FALSE, mvp.constData());
        glUniform4f(glGetUniformLocation(grid_program_, "u_color"),
                    0.35f, 0.35f, 0.35f, 0.6f);

        glBindVertexArray(grid_vao_);
        glDrawArrays(GL_LINES, 0, grid_vertex_count_);
        glBindVertexArray(0);
    }

    // --- Draw brush preview ---
    if (hover_col_ >= 0 && hover_row_ >= 0) {
        drawBrushPreview();
    }

    // --- Debug: draw center marker ---
    {
        // Compile a minimal test shader on first use
        static GLuint test_prog = 0;
        if (!test_prog) {
            GLuint vs = compileShader(GL_VERTEX_SHADER, kGridVert);
            GLuint fs = compileShader(GL_FRAGMENT_SHADER, kGridFrag);
            if (vs && fs) test_prog = linkProgram(vs, fs);
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
        }
        if (test_prog) {
            float cx = cam_center_.x(), cz = cam_center_.y();
            glUseProgram(test_prog);
            glUniformMatrix4fv(glGetUniformLocation(test_prog, "u_mvp"), 1, GL_FALSE, mvp.constData());
            auto draw_line = [&](float x1,float y1,float z1,float x2,float y2,float z2,float r,float g,float b,float a) {
                glUniform4f(glGetUniformLocation(test_prog, "u_color"), r,g,b,a);
                float verts[] = {x1,y1,z1, x2,y2,z2};
                GLuint vao2, vbo2;
                glGenVertexArrays(1, &vao2);
                glGenBuffers(1, &vbo2);
                glBindVertexArray(vao2);
                glBindBuffer(GL_ARRAY_BUFFER, vbo2);
                glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, 0);
                glEnableVertexAttribArray(0);
                glDrawArrays(GL_LINES, 0, 2);
                glDeleteVertexArrays(1, &vao2);
                glDeleteBuffers(1, &vbo2);
                glBindVertexArray(0);
            };
            draw_line(cx-30,0,cz, cx+30,0,cz, 1,0,0,1);
            draw_line(cx,0,cz-30, cx,0,cz+30, 0,0,1,1);
            err = glGetError();
            if (err != GL_NO_ERROR) qWarning() << "[TerrainEdit] marker error:" << err;
        }
    }

    glUseProgram(0);

    // --- Draw help overlay ---
    drawHelpOverlay();
}

void TerrainEditWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

// ============================================================
// Brush preview — follows terrain surface
// ============================================================
void TerrainEditWidget::drawBrushPreview() {
    if (!terrain_ || !grid_program_) return;

    float cx = terrain_->center_offset_x + hover_col_ * kTileSize;
    float cz = terrain_->center_offset_y + hover_row_ * kTileSize;
    float radius_world = brush_size_ * kTileSize;

    // Compute current MVP
    float aspect = static_cast<float>(width()) / static_cast<float>(qMax(height(), 1));
    float yaw   = qDegreesToRadians(cam_yaw_);
    float pitch = qDegreesToRadians(cam_pitch_);
    float cosP  = qCos(pitch);
    float sinP  = qAbs(qSin(pitch));  // always above terrain

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

    GLint prog = grid_program_;
    glUseProgram(prog);
    glUniformMatrix4fv(u_grid_mvp_, 1, GL_FALSE, mvp.constData());
    glUniform4f(glGetUniformLocation(prog, "u_color"),
                1.0f, 1.0f, 0.0f, 0.7f);

    int tw = terrain_->tile_width;

    if (brush_shape_ == BrushShape::Square) {
        // Draw square outline — 4 edges with terrain-following heights
        float hw = radius_world;
        float corners[4][2] = {
            {cx - hw, cz - hw}, {cx + hw, cz - hw},
            {cx + hw, cz + hw}, {cx - hw, cz + hw}
        };
        // 5 points to close the loop
        float verts[5][3];
        for (int i = 0; i < 5; ++i) {
            int ci = i % 4;
            float wx = corners[ci][0];
            float wz = corners[ci][1];
            float fc = (wx - terrain_->center_offset_x) / kTileSize;
            float fr = (wz - terrain_->center_offset_y) / kTileSize;
            int cci = qBound(0, qRound(fc), tw - 1);
            int rri = qBound(0, qRound(fr), terrain_->tile_height - 1);
            float wy = terrain_->tilepoints[rri * tw + cci].height + 5.0f;
            verts[i][0] = wx; verts[i][1] = wy; verts[i][2] = wz;
        }
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), verts);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINE_LOOP, 0, 5);
        glDisableVertexAttribArray(0);
    } else {
        // Build circle with terrain-following heights
        const int segs = 48;
        float verts[(segs + 1) * 3];

        for (int i = 0; i <= segs; ++i) {
            float a = 2.0f * 3.14159265f * i / segs;
            float wx = cx + radius_world * cosf(a);
            float wz = cz + radius_world * sinf(a);
            float fc = (wx - terrain_->center_offset_x) / kTileSize;
            float fr = (wz - terrain_->center_offset_y) / kTileSize;
            int ci = qBound(0, qRound(fc), tw - 1);
            int ri = qBound(0, qRound(fr), terrain_->tile_height - 1);
            float wy = terrain_->tilepoints[ri * tw + ci].height + 5.0f;
            verts[i * 3]     = wx;
            verts[i * 3 + 1] = wy;
            verts[i * 3 + 2] = wz;
        }

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), verts);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINE_LOOP, 0, segs + 1);
        glDisableVertexAttribArray(0);
    }
}

// ============================================================
// Help overlay — renders in top‑right corner via QPainter
// ============================================================
void TerrainEditWidget::drawHelpOverlay() {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int ow = 260;
    int oh = 250;
    int mx = width() - ow - 10;
    int my = 10;

    // Semi-transparent background
    p.fillRect(mx, my, ow, oh, QColor(0, 0, 0, 160));

    // Title
    p.setPen(QColor(255, 255, 200));
    QFont f = p.font();
    f.setPointSize(10);
    f.setBold(true);
    p.setFont(f);
    p.drawText(mx + 8, my + 16, tr("Controls"));

    // Help lines
    f.setPointSize(8);
    f.setBold(false);
    p.setFont(f);
    p.setPen(QColor(220, 220, 220));

    int y = my + 30;
    auto draw_help_line = [&](const char* key, const QString& desc) {
        QString text = QString::fromUtf8(key) + QStringLiteral("  ") + desc;
        p.drawText(mx + 8, y, text);
        y += 18;
    };

    draw_help_line("LMB",           tr("Edit terrain"));
    draw_help_line("MMB / Alt+LMB", tr("Orbit"));
    draw_help_line("RMB",           tr("Pan"));
    draw_help_line("Scroll",        tr("Zoom"));
    draw_help_line("1-5",           tr("Select tool"));
    draw_help_line("[ / ]",         tr("Brush size"));
    draw_help_line("Shift+[ / ]",   tr("Brush strength"));
    draw_help_line("B",             tr("Brush shape"));
    draw_help_line("Ctrl+Z",        tr("Undo"));
    draw_help_line("Ctrl+Y",        tr("Redo"));
    draw_help_line("F",             tr("Focus tile"));

    // Render mode indicator at bottom of overlay
    f.setPointSize(7);
    f.setBold(true);
    p.setFont(f);
    QString modeText;
    switch (render_mode_) {
    case RenderMode::Wireframe: modeText = tr("Wireframe"); break;
    case RenderMode::Unlit:     modeText = tr("Unlit");     break;
    case RenderMode::Lit:       modeText = tr("Lit");       break;
    }
    p.setPen(QColor(180, 200, 255));
    p.drawText(mx + 8, my + oh - 10,
               tr("Render mode") + QStringLiteral(": ") + modeText);

    p.end();
}

// ============================================================
// Mouse picking
// ============================================================
bool TerrainEditWidget::pickTile(int screen_x, int screen_y, int& col, int& row) {
    if (!has_terrain_ || !terrain_) return false;

    int w = width();
    int h = height();
    if (h == 0 || w == 0) return false;

    float ndc_x = (2.0f * screen_x / w) - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y / h);

    float aspect = (float)w / (float)h;
    float yaw   = qDegreesToRadians(cam_yaw_);
    float pitch = qDegreesToRadians(cam_pitch_);
    float cosP  = qCos(pitch);
    float sinP  = qAbs(qSin(pitch));  // always above terrain

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

    bool invertible = false;
    QMatrix4x4 inv_mvp = mvp.inverted(&invertible);
    if (!invertible) return false;

    QVector4D near_ws = inv_mvp * QVector4D(ndc_x, ndc_y, -1.0f, 1.0f);
    QVector4D far_ws  = inv_mvp * QVector4D(ndc_x, ndc_y, 1.0f, 1.0f);
    if (near_ws.w() == 0 || far_ws.w() == 0) return false;

    near_ws /= near_ws.w();
    far_ws  /= far_ws.w();

    QVector3D ray_origin(near_ws.x(), near_ws.y(), near_ws.z());
    QVector3D ray_dir = QVector3D(far_ws.x(), far_ws.y(), far_ws.z()) - ray_origin;
    ray_dir.normalize();

    if (qAbs(ray_dir.y()) < 0.0001f) return false;

    float t_step = kTileSize * 0.5f;
    float max_dist = cam_distance_ * 3.0f;

    for (float t = 0.0f; t < max_dist; t += t_step) {
        QVector3D pos = ray_origin + ray_dir * t;

        float fc = (pos.x() - terrain_->center_offset_x) / kTileSize;
        float fr = (pos.z() - terrain_->center_offset_y) / kTileSize;

        int ci = qRound(fc);
        int ri = qRound(fr);
        ci = qBound(0, ci, terrain_->tile_width - 1);
        ri = qBound(0, ri, terrain_->tile_height - 1);

        float h = terrain_->tilepoints[ri * terrain_->tile_width + ci].height;
        if (pos.y() <= h) {
            col = ci;
            row = ri;
            return true;
        }
    }

    return false;
}

// ============================================================
// Editing operations
// ============================================================
void TerrainEditWidget::applyBrush(int center_col, int center_row) {
    if (!has_terrain_ || !terrain_) return;

    int radius = brush_size_;
    int min_c = std::max(0, center_col - radius);
    int max_c = std::min(terrain_->tile_width - 1, center_col + radius);
    int min_r = std::max(0, center_row - radius);
    int max_r = std::min(terrain_->tile_height - 1, center_row + radius);

    for (int r = min_r; r <= max_r; ++r) {
        for (int c = min_c; c <= max_c; ++c) {
            float dr = (float)(r - center_row);
            float dc = (float)(c - center_col);
            float dist = std::sqrt(dr * dr + dc * dc);
            if (brush_shape_ != BrushShape::Square && dist > (float)radius) continue;

            switch (current_tool_) {
            case EditTool::Raise:
                raiseLower(c, r, dist, (float)radius, 1.0f);
                break;
            case EditTool::Lower:
                raiseLower(c, r, dist, (float)radius, -1.0f);
                break;
            case EditTool::Smooth:
                smoothOp(c, r, dist, (float)radius);
                break;
            case EditTool::Flatten:
                flattenOp(c, r, dist, (float)radius);
                break;
            case EditTool::Paint:
                paintOp(c, r, dist, (float)radius);
                break;
            }
        }
    }

    mesh_dirty_ = true;
}

// Clamp height to safe w3e short range (±8192 from ground 0)
static float clampHeight(float h) {
    return qBound(-8192.0f, h, 8192.0f);
}

void TerrainEditWidget::raiseLower(int col, int row, float dist,
                                   float radius, float dir)
{
    float falloff = 1.0f - (dist / radius);
    float delta = dir * brush_strength_ * 8.0f * falloff;
    int idx = row * terrain_->tile_width + col;
    terrain_->tilepoints[idx].height = clampHeight(terrain_->tilepoints[idx].height + delta);
}

void TerrainEditWidget::smoothOp(int col, int row, float dist, float radius)
{
    int w = terrain_->tile_width;
    int idx = row * w + col;

    float sum = 0;
    int count = 0;
    if (col > 0)                      { sum += terrain_->tilepoints[idx - 1].height; count++; }
    if (col < w - 1)                  { sum += terrain_->tilepoints[idx + 1].height; count++; }
    if (row > 0)                      { sum += terrain_->tilepoints[idx - w].height; count++; }
    if (row < terrain_->tile_height - 1) { sum += terrain_->tilepoints[idx + w].height; count++; }

    if (count == 0) return;
    float avg = sum / count;

    float falloff = 1.0f - (dist / radius);
    float delta = (avg - terrain_->tilepoints[idx].height) * brush_strength_ * falloff * 0.3f;
    terrain_->tilepoints[idx].height = clampHeight(terrain_->tilepoints[idx].height + delta);
}

void TerrainEditWidget::flattenOp(int col, int row, float dist, float radius)
{
    int idx = row * terrain_->tile_width + col;
    float falloff = 1.0f - (dist / radius);
    float delta = (flatten_target_ - terrain_->tilepoints[idx].height)
                  * brush_strength_ * falloff * 0.3f;
    terrain_->tilepoints[idx].height = clampHeight(terrain_->tilepoints[idx].height + delta);
}

void TerrainEditWidget::paintOp(int col, int row, float /*dist*/, float /*radius*/)
{
    int idx = row * terrain_->tile_width + col;
    terrain_->tilepoints[idx].ground_texture = (uint8_t)paint_texture_;
}

// ============================================================
// Undo / Redo
// ============================================================
void TerrainEditWidget::pushUndo() {
    if (!terrain_) return;
    undo_stack_.push_back(*terrain_);
    if ((int)undo_stack_.size() > kMaxUndo_)
        undo_stack_.erase(undo_stack_.begin());
    redo_stack_.clear();
}

void TerrainEditWidget::undo() {
    if (!terrain_ || undo_stack_.empty()) return;
    redo_stack_.push_back(*terrain_);
    *terrain_ = undo_stack_.back();
    undo_stack_.pop_back();
    mesh_dirty_ = true;
    hover_col_ = -1; hover_row_ = -1;
    editing_ = false;
    emit terrainEdited();
    update();
}

void TerrainEditWidget::redo() {
    if (!terrain_ || redo_stack_.empty()) return;
    undo_stack_.push_back(*terrain_);
    *terrain_ = redo_stack_.back();
    redo_stack_.pop_back();
    mesh_dirty_ = true;
    hover_col_ = -1; hover_row_ = -1;
    editing_ = false;
    emit terrainEdited();
    update();
}

void TerrainEditWidget::focusOnTile(int col, int row) {
    if (!terrain_) return;
    col = qBound(0, col, terrain_->tile_width - 1);
    row = qBound(0, row, terrain_->tile_height - 1);
    float wx = terrain_->center_offset_x + col * kTileSize;
    float wz = terrain_->center_offset_y + row * kTileSize;
    cam_center_ = QPointF(wx, wz);
    cam_distance_ = qMax(cam_distance_ * 0.5f, 100.0f);
    hover_col_ = col;
    hover_row_ = row;
    update();
}

// ============================================================
// Mouse events
// ============================================================
void TerrainEditWidget::mousePressEvent(QMouseEvent* event) {
    if (!has_terrain_) return;

    if (event->button() == Qt::LeftButton &&
        !(QApplication::keyboardModifiers() & Qt::AltModifier))
    {
        int col, row;
        if (pickTile((int)event->position().x(), (int)event->position().y(), col, row)) {
            pushUndo();
            editing_ = true;
            hover_col_ = col;
            hover_row_ = row;

            if (current_tool_ == EditTool::Flatten) {
                flatten_target_ = terrain_->tilepoints[row * terrain_->tile_width + col].height;
            }

            applyBrush(col, row);
            emit terrainEdited();
            update();
        }
        return;
    }

    dragging_ = true;
    drag_button_ = event->button();
    last_mouse_pos_ = event->position();
}

void TerrainEditWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!has_terrain_ || !terrain_) return;

    if (editing_) {
        int col, row;
        if (pickTile((int)event->position().x(), (int)event->position().y(), col, row)) {
            bool changed = (col != hover_col_ || row != hover_row_);
            hover_col_ = col;
            hover_row_ = row;
            applyBrush(col, row);
            if (changed) {
                emit terrainEdited();
            }
            update();
        }
        return;
    }

    if (!dragging_) {
        int col, row;
        if (pickTile((int)event->position().x(), (int)event->position().y(), col, row)) {
            if (col != hover_col_ || row != hover_row_) {
                hover_col_ = col;
                hover_row_ = row;
                update();
            }
            // Show tile info in status
            int idx = row * terrain_->tile_width + col;
            const auto& tp = terrain_->tilepoints[idx];
            QString texName;
            if (tp.ground_texture < (int)terrain_->ground_tiles.size())
                texName = QString::fromStdString(terrain_->ground_tiles[tp.ground_texture]);
            else
                texName = QString::number(tp.ground_texture);
            emit statusMessage(
                QStringLiteral("Tile (%1, %2)  H: %3  Tex: %4")
                    .arg(col).arg(row)
                    .arg(tp.height, 0, 'f', 1)
                    .arg(texName));
        } else {
            if (hover_col_ >= 0 || hover_row_ >= 0) {
                hover_col_ = -1;
                hover_row_ = -1;
                update();
            }
            emit statusMessage(QString());
        }
        return;
    }

    // Camera dragging
    QPointF pos = event->position();
    float dx = static_cast<float>(pos.x() - last_mouse_pos_.x());
    float dy = static_cast<float>(pos.y() - last_mouse_pos_.y());
    last_mouse_pos_ = pos;

    if (drag_button_ == Qt::MiddleButton ||
        (drag_button_ == Qt::LeftButton && (QApplication::keyboardModifiers() & Qt::AltModifier)))
    {
        cam_yaw_   += dx * 0.3f;
        cam_pitch_ += dy * 0.3f;
        cam_pitch_  = qBound(-89.0f, cam_pitch_, -15.0f);
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

void TerrainEditWidget::mouseReleaseEvent(QMouseEvent* /*event*/) {
    editing_ = false;
    dragging_ = false;
}

void TerrainEditWidget::wheelEvent(QWheelEvent* event) {
    if (!has_terrain_) return;
    float delta = static_cast<float>(event->angleDelta().y()) / 120.0f;
    cam_distance_ *= (1.0f - delta * 0.1f);
    cam_distance_ = qBound(10.0f, cam_distance_, 100000.0f);

    int new_step = 1;
    if (cam_distance_ > 10000.0f) new_step = 8;
    else if (cam_distance_ > 5000.0f) new_step = 4;
    else if (cam_distance_ > 2000.0f) new_step = 2;

    if (new_step != g_last_grid_step) {
        g_last_grid_step = new_step;
        mesh_dirty_ = true;
    }
    event->accept();
    update();
}
