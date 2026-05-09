#pragma once

#include "../../gui/terrain_renderer_base.h"
#include "terrain_edit_types.h"
#include "../../src/core/w3e.h"
#include <QImage>
#include <vector>

class Wc3Manager;
class MapBuilder;

class TerrainEditWidget : public TerrainRendererBase {
    Q_OBJECT
public:
    explicit TerrainEditWidget(QWidget* parent = nullptr);
    ~TerrainEditWidget() override;

    void loadTerrain(Terrain* terrain);
    void setBuilder(MapBuilder* builder) { builder_ = builder; }
    void setWc3Manager(Wc3Manager* mgr) { TerrainRendererBase::setWc3Manager(mgr); }

    void setTool(EditTool tool) { current_tool_ = tool; update(); }
    void setBrushSize(int size) { brush_size_ = std::max(1, size); update(); }
    void setBrushStrength(float strength) { brush_strength_ = strength; }
    void setPaintTexture(int idx) { paint_texture_ = idx; update(); }
    void setShowTexture(bool on) { show_texture_ = on; update(); }
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
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void onBeforePaint() override;
    void onAfterPaint() override;
    void destroyGPUBuffers() override;

private:
    // Rendering helpers
    void drawBrushPreview();
    void drawCenterMarker();
    void drawHelpOverlay();

    // Editing
    bool pickTile(int screen_x, int screen_y, int& col, int& row);
    void applyBrush(int center_col, int center_row);
    void pushUndo();
    void raiseLower(int col, int row, float dist, float radius, float dir);
    void smoothOp(int col, int row, float dist, float radius);
    void flattenOp(int col, int row, float dist, float radius);
    void paintOp(int col, int row, float dist, float radius);

    // Color helpers
    QColor heightColor(float h) const;
    QColor textureColor(int tex_idx) const;
    QColor tileColor(const QString& tileId, int fallback_idx) const;

    // Tool state
    EditTool current_tool_ = EditTool::Raise;
    int brush_size_ = 4;
    float brush_strength_ = 1.0f;
    int paint_texture_ = 0;
    BrushShape brush_shape_ = BrushShape::Circle;
    RenderMode render_mode_ = RenderMode::Lit;
    int hover_col_ = -1;
    int hover_row_ = -1;
    bool editing_ = false;
    float flatten_target_ = 0;

    static constexpr int kMaxUndo_ = 60;

    // Undo / redo stacks
    std::vector<Terrain> undo_stack_;
    std::vector<Terrain> redo_stack_;
};
