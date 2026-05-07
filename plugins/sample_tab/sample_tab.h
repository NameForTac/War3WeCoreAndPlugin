#pragma once

#include "../../src/core/plugin.h"

#include <QTreeWidget>
#include <QWidget>
#include <map>
#include <string>

// ============================================================
// SampleTabPlugin — a comprehensive DLL plugin that reads and
// displays all map data in a categorized tree view.
//
// Categories: Map Info, Terrain, Objects, Units, Doodads,
//             Strings, Files
//
// TRIGSTR_XXX references are resolved from the WTS string table
// automatically in all displayed text.
// ============================================================
class SampleTabPlugin : public IEditorPlugin {
public:
    SampleTabPlugin() = default;
    ~SampleTabPlugin() override = default;

    // Identity
    QString name() const override { return "Map Data Viewer"; }
    QString version() const override { return "1.0.0"; }
    PluginCapability capabilities() const override {
        return PluginCapability::ProvidesTab;
    }

    // Lifecycle
    bool init(PluginContext& ctx) override;
    void activate() override;
    void deactivate() override;
    void destroy() override;

    // Tab
    QWidget* tab_widget() override { return widget_; }
    QString tab_title() override { return "Data Viewer"; }

private:
    void populate_all();
    void populate_map_info();
    void populate_terrain();
    void populate_objects();
    void populate_units();
    void populate_doodads();
    void populate_strings();
    void populate_files();

    QString resolve(const std::string& text) const;

    QTreeWidgetItem* add_category(const QString& title);
    QTreeWidgetItem* add_subcategory(QTreeWidgetItem* parent, const QString& title);
    void add_item(QTreeWidgetItem* parent, const QString& key, const QString& value);

    static QString player_type_str(int type);
    static QString race_str(int race);
    static QString flag_desc(int flags);
    static QString format_file_size(int64_t bytes);

    QWidget* widget_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    PluginContext ctx_;
    std::map<int, std::string> wts_;  // loaded WTS for TRIGSTR_XXX resolution
};
