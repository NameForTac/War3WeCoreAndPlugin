#pragma once

#include "../src/core/w3e.h"
#include "../src/core/plugin.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMatrix4x4>
#include <QVector3D>
#include <memory>

class MapBuilder;

// ============================================================
// TerrainWidget — QOpenGLWidget that renders the 3D terrain
// ============================================================
class TerrainWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    explicit TerrainWidget(QWidget* parent = nullptr);
    ~TerrainWidget() override;

    void load_terrain(const Terrain& terrain);
    void clear_terrain();

signals:
    void contentChanged();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void build_mesh();
    void upload_mesh();
    QColor height_color(float h) const;
    GLuint compile_shader(GLenum type, const char* src);
    GLuint link_program(GLuint vs, GLuint fs);

    // Terrain data (owned)
    std::unique_ptr<Terrain> terrain_;

    // Camera state
    float cam_yaw_ = -45.0f;
    float cam_pitch_ = -35.0f;
    float cam_distance_ = 800.0f;
    QPointF cam_center_; // world XZ center of the map

    // Interaction state
    bool dragging_ = false;
    int drag_button_ = 0;
    QPointF last_mouse_pos_;

    // OpenGL state
    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    int vertex_count_ = 0;
    int index_count_ = 0;

    // Uniform locations
    GLuint u_mvp_ = 0;
    GLuint u_water_level_ = 0;

    // Whether terrain is loaded
    bool has_terrain_ = false;
};

// ============================================================
// TerrainEditorPlugin — IEditorPlugin wrapper for TerrainWidget
// ============================================================
class TerrainEditorPlugin : public QObject, public IEditorPlugin {
    Q_OBJECT
public:
    QString name() const override { return tr("Terrain Editor"); }
    QString version() const override { return "1.0"; }
    PluginCapability capabilities() const override {
        return PluginCapability::ProvidesTab;
    }

    bool init(PluginContext& ctx) override;
    void activate() override;
    void deactivate() override;
    void destroy() override;

    QWidget* tab_widget() override { return widget_; }
    QString tab_title() override { return tr("Terrain Editor"); }

private:
    TerrainWidget* widget_ = nullptr;
    MapBuilder* builder_ = nullptr;
};
