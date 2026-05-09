#pragma once

#include "../src/core/w3e.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_3_Core>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMatrix4x4>
#include <QVector3D>
#include <QPointF>
#include <QImage>
#include <vector>
#include <memory>

class MapBuilder;
class Wc3Manager;

// Per-tile texture data uploaded to GPU SSBO (std430 layout, 16 bytes per tile)
struct alignas(16) TileTextureGPU {
    uint32_t layers[4];
};

// ============================================================
// TerrainRendererBase — shared HiveWE-style GPU terrain rendering
//
// Both TerrainWidget (built-in tab) and TerrainEditWidget
// (plugin) inherit from this to avoid duplicating shaders,
// SSBO management, texture loading, and camera controls.
//
// Derived classes:
//   - Set terrain_ pointer + has_terrain_ when terrain is loaded
//   - Override onMousePress/Move/Release for editing tools
//   - Add custom rendering in onBeforePaint/onAfterPaint
// ============================================================
class TerrainRendererBase : public QOpenGLWidget, protected QOpenGLFunctions_4_3_Core {
    Q_OBJECT
public:
    explicit TerrainRendererBase(QWidget* parent = nullptr);
    ~TerrainRendererBase() override;

    void setShowTexture(bool on) { show_texture_ = on; update(); }
    void setBuilder(MapBuilder* b) { builder_ = b; }
    void setWc3Manager(Wc3Manager* mgr) { wc3_ = mgr; tex_dirty_ = true; update(); }

signals:
    void contentChanged();

protected:
    // Override points for derived classes
    virtual void onTerrainLoaded() {}
    virtual void onBeforePaint() {}
    virtual void onAfterPaint() {}
    // Mouse: call baseCameraPress/Move/Release from overrides if keeping orbit
    virtual void onMousePress(QMouseEvent* event) {}
    virtual void onMouseMove(QMouseEvent* event) {}
    virtual void onMouseRelease(QMouseEvent* event) {}

    // Base camera handlers (call from overrides when not editing)
    void baseMousePress(QMouseEvent* event);
    void baseMouseMove(QMouseEvent* event);
    void baseMouseRelease(QMouseEvent* event);
    void baseWheelEvent(QWheelEvent* event);

    // OpenGL
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    // Shader helpers
    GLuint compileShader(GLenum type, const char* src);
    GLuint linkProgram(GLuint vs, GLuint fs);
    bool initTerrainShaders();
    void initGridShaders();

    // GPU buffer management (HiveWE-style SSBO)
    void createGPUBuffers();
    virtual void destroyGPUBuffers();
    void updateGroundHeights(const QRect& area);
    void updateGroundTextures(const QRect& area);

    // Texture management
    void uploadTextures();
    QImage generateTileTexture(const QString& tileId, int idx);
    QImage generateNoiseTexture(int w, int h, const QColor& base, const QColor& var, float scale);
    void freeTextures();
    uint8_t realTileTexture(int x, int y) const;
    int getTileVariant(int tex_idx, int variation) const;

    // Height conversion: raw int16 (0x2000 = ground 0) to world Y
    static float rawToWorldY(float raw_h) {
        return (raw_h - 8192.0f) * 0.25f;
    }

    // Render terrain (called by paintGL)
    void renderTerrain(const QMatrix4x4& mvp);

    // Compute MVP matrix from current camera
    QMatrix4x4 computeMVPMatrix(float aspect) const;

    // Terrain data (non-owning — derived classes manage lifetime)
    Terrain* terrain_ = nullptr;

    // Camera state
    float cam_yaw_ = -45.0f;
    float cam_pitch_ = -35.0f;
    float cam_distance_ = 800.0f;
    QPointF cam_center_;

    // Interaction state
    bool dragging_ = false;
    int drag_button_ = 0;
    QPointF last_mouse_pos_;

    // Terrain shader
    GLuint program_ = 0;
    GLuint u_mvp_ = 0;
    GLuint u_map_size_ = 0;
    GLuint u_origin_ = 0;
    GLuint u_light_dir_ = 0;
    GLuint u_lighting_ = 0;
    GLuint u_use_tex_ = 0;
    GLuint u_tex_array_ = 0;

    // SSBOs
    GLuint height_ssbo_ = 0;
    GLuint cliff_level_ssbo_ = 0;
    GLuint texture_data_ssbo_ = 0;

    // CPU copies for SSBO upload
    std::vector<float> gpu_heights_;
    std::vector<float> gpu_final_heights_;
    std::vector<TileTextureGPU> gpu_texture_data_;

    // Texture array
    GLuint tex_array_ = 0;
    int tex_count_ = 0;
    int tile_size_ = 64;
    std::vector<int> layer_offsets_;
    std::vector<bool> tex_extended_;

    // Dummy VAO for SSBO instanced rendering (core profile requires a bound VAO)
    GLuint vao_ = 0;

    // Grid shader (used by both for optional grid)
    GLuint grid_program_ = 0;
    GLuint u_grid_mvp_ = 0;

    // State
    bool has_terrain_ = false;
    bool tex_dirty_ = false;
    bool show_texture_ = true;

    // External references for texture loading
    MapBuilder* builder_ = nullptr;
    Wc3Manager* wc3_ = nullptr;

    // Cached MVP for derived class overlay rendering
    QMatrix4x4 last_mvp_;

    static constexpr float kTileSize = 128.0f;
};
