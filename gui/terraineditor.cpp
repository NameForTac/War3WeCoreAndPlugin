#include "terraineditor.h"
#include "../src/core/map_builder.h"
#include <QOpenGLContext>
#include <QtMath>
#include <QApplication>
#include <algorithm>

// ============================================================
// GLSL shaders
// ============================================================
static const char* kVertShader = R"(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_color;
uniform mat4 u_mvp;
out vec3 v_color;
void main() {
    v_color = a_color;
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}
)";

static const char* kFragShader = R"(
#version 330 core
in vec3 v_color;
out vec4 frag_color;
void main() {
    frag_color = vec4(v_color, 1.0);
}
)";

// Water fragment shader (fixed blue with alpha)
static const char* kFragWater = R"(
#version 330 core
out vec4 frag_color;
void main() {
    frag_color = vec4(0.2, 0.4, 0.8, 0.45);
}
)";

static constexpr float kTileSize = 128.0f;

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
    if (program_)  glDeleteProgram(program_);
    if (vbo_)      glDeleteBuffers(1, &vbo_);
    if (ebo_)      glDeleteBuffers(1, &ebo_);
    if (vao_)      glDeleteVertexArrays(1, &vao_);
    doneCurrent();
}

void TerrainWidget::load_terrain(const Terrain& terrain) {
    terrain_ = std::make_unique<Terrain>(terrain);
    has_terrain_ = true;

    // Compute camera center (center of the map in world space)
    float cx = terrain_->center_offset_x + (terrain_->tile_width - 1) * kTileSize * 0.5f;
    float cz = terrain_->center_offset_y + (terrain_->tile_height - 1) * kTileSize * 0.5f;
    cam_center_ = QPointF(cx, cz);

    // Auto-fit camera distance
    float max_dim = static_cast<float>(qMax(terrain_->tile_width, terrain_->tile_height)) * kTileSize;
    cam_distance_ = max_dim * 0.8f;
    cam_distance_ = qBound(100.0f, cam_distance_, 10000.0f);

    update();
}

void TerrainWidget::clear_terrain() {
    has_terrain_ = false;
    terrain_.reset();
    vertex_count_ = 0;
    index_count_ = 0;
    update();
}

void TerrainWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
        qWarning() << "Shader compile error:" << log;
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
        qWarning() << "Program link error:" << log;
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

QColor TerrainWidget::height_color(float h) const {
    // Height range: find min/max from terrain or use defaults
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

    // Three-stop gradient: dark green / brown / light
    if (t < 0.5f) {
        float s = t * 2.0f;
        return QColor::fromRgbF(
            0.2f + s * 0.4f,
            0.4f + s * 0.2f,
            0.1f + s * 0.1f);
    } else {
        float s = (t - 0.5f) * 2.0f;
        return QColor::fromRgbF(
            0.6f + s * 0.3f,
            0.6f + s * 0.3f,
            0.2f + s * 0.6f);
    }
}

void TerrainWidget::build_mesh() {
    if (!terrain_) return;

    int cols = terrain_->tile_width;
    int rows = terrain_->tile_height;

    // Vertices: one per tile point
    struct Vertex { float x, y, z; float r, g, b; };
    std::vector<Vertex> vertices;
    vertices.reserve(cols * rows);

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            int idx = row * cols + col;
            const auto& tp = terrain_->tilepoints[idx];

            float wx = terrain_->center_offset_x + col * kTileSize;
            float wz = terrain_->center_offset_y + row * kTileSize;
            float wy = tp.height;

            QColor c = height_color(tp.height);
            vertices.push_back({wx, wy, wz, (float)c.redF(), (float)c.greenF(), (float)c.blueF()});
        }
    }

    // Indices: 2 triangles per quad (6 indices)
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

            // Triangle 1: bl-br-tl
            indices.push_back(bl);
            indices.push_back(br);
            indices.push_back(tl);
            // Triangle 2: br-tr-tl
            indices.push_back(br);
            indices.push_back(tr);
            indices.push_back(tl);
        }
    }

    vertex_count_ = (int)vertices.size();
    index_count_ = (int)indices.size();

    // Upload to GPU
    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
                 indices.data(), GL_STATIC_DRAW);

    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(0);

    // Color attribute (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    glEnableVertexAttribArray(1);

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

    if (!has_terrain_) return;

    // Compile shaders
    GLuint vs = compile_shader(GL_VERTEX_SHADER, kVertShader);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kFragShader);
    if (vs && fs) {
        program_ = link_program(vs, fs);
        if (program_) {
            u_mvp_ = glGetUniformLocation(program_, "u_mvp");
        }
    }
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);

    build_mesh();
}

void TerrainWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!has_terrain_ || !program_ || vertex_count_ == 0)
        return;

    // Compute camera matrices
    float aspect = static_cast<float>(width()) / static_cast<float>(qMax(height(), 1));

    float yaw   = qDegreesToRadians(cam_yaw_);
    float pitch = qDegreesToRadians(cam_pitch_);
    float cosP  = qCos(pitch);
    float sinP  = qSin(pitch);

    QVector3D eye(
        cam_center_.x() + cam_distance_ * qCos(yaw) * cosP,
        cam_distance_ * sinP,
        cam_center_.y() + cam_distance_ * qSin(yaw) * cosP
    );

    // Clamp pitch so camera doesn't flip
    if (sinP > 0.999f) eye.setY(cam_distance_);
    if (sinP < -0.999f) eye.setY(-cam_distance_);

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

    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

}

void TerrainWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

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
        // Orbit: yaw + pitch
        cam_yaw_ += dx * 0.3f;
        cam_pitch_ += dy * 0.3f;
        cam_pitch_ = qBound(-89.0f, cam_pitch_, 89.0f);
        update();
    } else if (drag_button_ == Qt::RightButton) {
        // Pan: move center in screen space
        float scale = cam_distance_ * 0.002f;
        float yaw = qDegreesToRadians(cam_yaw_);
        float pitch = qDegreesToRadians(cam_pitch_);

        QVector3D forward(qCos(yaw) * qCos(pitch), 0, qSin(yaw) * qCos(pitch));
        QVector3D right(-qSin(yaw), 0, qCos(yaw));

        forward.setY(0);
        forward.normalize();
        right.setY(0);
        right.normalize();

        cam_center_ -= QPointF(right.x() * dx * scale, right.z() * dx * scale);
        cam_center_ += QPointF(forward.x() * dy * scale, forward.z() * dy * scale);
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

