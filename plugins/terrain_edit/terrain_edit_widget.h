#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMatrix4x4>
#include <QVector3D>
#include <QImage>
#include <vector>
#include <string>
#include "terrain_edit_types.h"
#include "../../src/core/w3e.h"

class Wc3Manager;

class MapBuilder;

class TerrainEditWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    explicit TerrainEditWidget(QWidget* parent = nullptr);
    ~TerrainEditWidget() override;

    void loadTerrain(Terrain* terrain);
    void setBuilder(MapBuilder* builder) { builder_ = builder; }
    void setWc3Manager(Wc3Manager* mgr) { wc3_ = mgr; tex_dirty_ = true; update(); }

    void setTool(EditTool tool) { current_tool_ = tool; update(); }
    void setBrushSize(int size) { brush_size_ = std::max(1, size); update(); }
    void setBrushStrength(float strength) { brush_strength_ = strength; }
    void setPaintTexture(int idx) { paint_texture_ = idx; update(); }
    void setShowTexture(bool on) { show_texture_ = on; mesh_dirty_ = true; update(); }
    void setBrushShape(BrushShape shape) { brush_shape_ = shape; update(); }
    BrushShape brushShape() const { return brush_shape_; }
    void setRenderMode(RenderMode mode) { render_mode_ = mode; update(); }
    RenderMode renderMode() const { return render_mode_; }
    void undo();
    void redo();
    void focusOnTile(int col, int row);
    int hoverCol() const { return hover_col_; }
    int hoverRow() const { return hover_row_; }

    EditTool tool() const { return current_tool_; }
    int brushSize() const { return brush_size_; }
    float brushStrength() const { return brush_strength_; }
    int paintTexture() const { return paint_texture_; }

signals:
    void terrainEdited();
    void statusMessage(const QString& msg);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    // Rendering
    void buildMesh();
    void uploadMesh();
    void uploadGrid();
    void drawBrushPreview();

    // Shaders
    GLuint compileShader(GLenum type, const char* src);
    GLuint linkProgram(GLuint vs, GLuint fs);

    // Textures
    void uploadTextures();
    QImage generateTileTexture(const QString& tileId, int idx);
    QImage generateNoiseTexture(int w, int h, const QColor& base, const QColor& var, float scale);
    void freeTextures();

    // Help overlay
    void drawHelpOverlay();

    // Editing
    bool pickTile(int screen_x, int screen_y, int& col, int& row);
    void applyBrush(int center_col, int center_row);
    void pushUndo();
    void raiseLower(int col, int row, float dist, float radius, float dir);
    void smoothOp(int col, int row, float dist, float radius);
    void flattenOp(int col, int row, float dist, float radius);
    void paintOp(int col, int row, float dist, float radius);

    QColor heightColor(float h) const;
    QColor textureColor(int tex_idx) const;
    QColor tileColor(const QString& tileId, int fallback_idx) const;

    // Terrain data (pointer to window's copy)
    Terrain* terrain_ = nullptr;
    MapBuilder* builder_ = nullptr;
    Wc3Manager* wc3_ = nullptr;

    // Camera
    float cam_yaw_ = -45.0f;
    float cam_pitch_ = -35.0f;
    float cam_distance_ = 800.0f;
    QPointF cam_center_;

    // Interaction state
    bool dragging_ = false;
    int drag_button_ = 0;
    QPointF last_mouse_pos_;
    int hover_col_ = -1;
    int hover_row_ = -1;
    bool editing_ = false;

    // Flatten target height (captured on first click)
    float flatten_target_ = 0;

    // OpenGL state
    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    int vertex_count_ = 0;
    int index_count_ = 0;
    GLuint u_mvp_ = 0;
    GLuint u_light_dir_ = 0;
    GLuint u_lighting_ = 0;
    GLuint u_use_tex_ = 0;
    GLuint u_tex_array_ = 0;

    GLuint tex_array_ = 0;
    int tex_count_ = 0;
    int tile_size_ = 64;
    std::vector<int> layer_offsets_;
    std::vector<bool> tex_extended_;

    GLuint grid_vao_ = 0;
    GLuint grid_vbo_ = 0;
    int grid_vertex_count_ = 0;
    GLuint grid_program_ = 0;
    GLuint u_grid_mvp_ = 0;

    // Tool state
    EditTool current_tool_ = EditTool::Raise;
    int brush_size_ = 4;
    float brush_strength_ = 1.0f;
    int paint_texture_ = 0;
    BrushShape brush_shape_ = BrushShape::Circle;
    RenderMode render_mode_ = RenderMode::Lit;
    bool show_texture_ = true;
    bool has_terrain_ = false;
    bool mesh_dirty_ = false;
    bool tex_dirty_ = false;

    static constexpr float kTileSize = 128.0f;
    static constexpr int kMaxUndo_ = 60;

    // Undo / redo stacks (each holds a full Terrain copy)
    std::vector<Terrain> undo_stack_;
    std::vector<Terrain> redo_stack_;
};
