#pragma once

#include "../src/core/w3e.h"
#include "../src/core/plugin.h"
#include "terrain_renderer_base.h"
#include <memory>

class MapBuilder;
class Wc3Manager;

// ============================================================
// TerrainWidget — lightweight widget inheriting shared rendering
// from TerrainRendererBase. Owns its terrain copy.
// ============================================================
class TerrainWidget : public TerrainRendererBase {
    Q_OBJECT
public:
    explicit TerrainWidget(QWidget* parent = nullptr);
    ~TerrainWidget() override;

    void load_terrain(const Terrain& terrain);
    void clear_terrain();
    void set_builder(MapBuilder* builder) { builder_ = builder; }
    void set_wc3_manager(Wc3Manager* mgr) { setWc3Manager(mgr); }

private:
    std::unique_ptr<Terrain> owned_terrain_;
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
