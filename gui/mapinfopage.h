#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTableWidget>
#include <vector>

#include "../src/core/plugin.h"

struct MapInfo;
struct PlayerInfo;
struct ForceInfo;

class MapBuilder;

class MapInfoPage : public QWidget {
    Q_OBJECT
public:
    explicit MapInfoPage(QWidget* parent = nullptr);

    void load(const MapInfo& info);
    void collect_into(MapInfo& info) const;
    void clear();

private:
    void build_ui();
    QWidget* create_basic_group();
    QWidget* create_loading_group();
    QWidget* create_prologue_group();
    QWidget* create_environment_group();
    QWidget* create_players_group();
    QWidget* create_forces_group();

    void load_players(const std::vector<PlayerInfo>& players);
    void load_forces(const std::vector<ForceInfo>& forces);
    std::vector<PlayerInfo> collect_players() const;
    std::vector<ForceInfo> collect_forces() const;

signals:
    void contentChanged();

private:
    // Basic
    QLineEdit* name_edit_ = nullptr;
    QLineEdit* author_edit_ = nullptr;
    QTextEdit* desc_edit_ = nullptr;
    QLineEdit* players_desc_edit_ = nullptr;
    QSpinBox* map_version_spin_ = nullptr;
    QSpinBox* map_width_spin_ = nullptr;
    QSpinBox* map_height_spin_ = nullptr;

    // Camera
    QDoubleSpinBox* cam_left_ = nullptr;
    QDoubleSpinBox* cam_right_ = nullptr;
    QDoubleSpinBox* cam_bottom_ = nullptr;
    QDoubleSpinBox* cam_top_ = nullptr;

    // Loading screen
    QComboBox* loading_type_ = nullptr;
    QLineEdit* loading_path_ = nullptr;
    QTextEdit* loading_text_ = nullptr;
    QLineEdit* loading_title_ = nullptr;
    QLineEdit* loading_subtitle_ = nullptr;

    // Prologue
    QLineEdit* prologue_path_ = nullptr;
    QTextEdit* prologue_text_ = nullptr;
    QLineEdit* prologue_title_ = nullptr;
    QLineEdit* prologue_subtitle_ = nullptr;

    // Environment
    QComboBox* fog_type_ = nullptr;
    QDoubleSpinBox* fog_start_ = nullptr;
    QDoubleSpinBox* fog_end_ = nullptr;
    QDoubleSpinBox* fog_density_ = nullptr;
    QSpinBox* fog_r_ = nullptr;
    QSpinBox* fog_g_ = nullptr;
    QSpinBox* fog_b_ = nullptr;
    QSpinBox* water_r_ = nullptr;
    QSpinBox* water_g_ = nullptr;
    QSpinBox* water_b_ = nullptr;
    QComboBox* script_type_ = nullptr;

    // Tables
    QTableWidget* players_table_ = nullptr;
    QTableWidget* forces_table_ = nullptr;
    bool loading_ = false;
};

// ============================================================
// MapInfoPlugin — IEditorPlugin wrapper for MapInfoPage
// ============================================================
class MapInfoPlugin : public QObject, public IEditorPlugin {
    Q_OBJECT
public:
    QString name() const override { return tr("Map Info"); }
    QString version() const override { return "1.0"; }
    PluginCapability capabilities() const override {
        return PluginCapability::ProvidesTab | PluginCapability::NeedsSavable;
    }

    bool init(PluginContext& ctx) override;
    void activate() override;
    void deactivate() override;
    void destroy() override;

    QWidget* tab_widget() override { return page_; }
    QString tab_title() override { return tr("Map Info"); }
    void sync_to_builder() override;

signals:
    void contentChanged();

private:
    MapInfoPage* page_ = nullptr;
    MapBuilder* builder_ = nullptr;
};
