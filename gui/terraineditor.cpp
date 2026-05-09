#include "terraineditor.h"
#include "../src/core/map_builder.h"
#include <QtMath>

// ============================================================
// TerrainWidget
// ============================================================
TerrainWidget::TerrainWidget(QWidget* parent)
    : TerrainRendererBase(parent)
{
}

TerrainWidget::~TerrainWidget() {
    // Base destructor handles GL resource cleanup
}

void TerrainWidget::load_terrain(const Terrain& terrain) {
    owned_terrain_ = std::make_unique<Terrain>(terrain);
    terrain_ = owned_terrain_.get();
    has_terrain_ = true;
    tex_dirty_ = true;

    float cx = terrain_->center_offset_x + (terrain_->tile_width - 1) * kTileSize * 0.5f;
    float cz = terrain_->center_offset_y + (terrain_->tile_height - 1) * kTileSize * 0.5f;
    cam_center_ = QPointF(cx, cz);

    float max_dim = static_cast<float>(qMax(terrain_->tile_width, terrain_->tile_height)) * kTileSize;
    cam_distance_ = qBound(100.0f, max_dim * 0.8f, 10000.0f);

    update();
}

void TerrainWidget::clear_terrain() {
    makeCurrent();
    destroyGPUBuffers();
    owned_terrain_.reset();
    terrain_ = nullptr;
    has_terrain_ = false;
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
