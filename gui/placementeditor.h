#pragma once

#include "../src/core/plugin.h"
#include "../src/core/doo.h"
#include "../src/core/units_doo.h"
#include <QWidget>
#include <QPainter>
#include <memory>

class MapBuilder;
class MetaDataDB;

// ============================================================
// PlacementWidget — 2D top-down view of placed units & doodads
// ============================================================
class PlacementWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlacementWidget(QWidget* parent = nullptr);

    void load_data(MapBuilder* builder);
    void clear_data();
    void set_meta_db(MetaDataDB* db) { meta_db_ = db; }

signals:
    void contentChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QPointF world_to_screen(float wx, float wy) const;
    QColor player_color(int owner) const;

    MetaDataDB* meta_db_ = nullptr;

    // Unit display info
    struct UnitInfo {
        float x = 0, y = 0;
        std::string type_id;
        int owner = 0;
        bool is_doodad = false;
    };
    std::vector<UnitInfo> units_;
    int hovered_idx_ = -1;

    bool has_data_ = false;

    // World bounds
    float world_min_x_ = 0, world_min_y_ = 0;
    float world_max_x_ = 0, world_max_y_ = 0;
    float scale_ = 1.0f;
    int margin_ = 20;
};

// ============================================================
// PlacementEditorPlugin — IEditorPlugin wrapper
// ============================================================
class PlacementEditorPlugin : public QObject, public IEditorPlugin {
    Q_OBJECT
public:
    PlacementEditorPlugin();
    ~PlacementEditorPlugin() override;

    QString name() const override { return tr("Placement Editor"); }
    QString version() const override { return "0.1"; }
    PluginCapability capabilities() const override {
        return PluginCapability::ProvidesTab | PluginCapability::NeedsSavable;
    }

    bool init(PluginContext& ctx) override;
    void activate() override;
    void deactivate() override;
    void destroy() override;

    QWidget* tab_widget() override { return widget_; }
    QString tab_title() override { return tr("Placement Editor"); }

    void sync_to_builder() override {}

private:
    PlacementWidget* widget_ = nullptr;
    MapBuilder* builder_ = nullptr;
};
