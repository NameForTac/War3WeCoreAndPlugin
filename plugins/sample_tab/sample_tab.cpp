#include "sample_tab.h"

#include "../../src/core/map_builder.h"
#include "../../src/core/metadata.h"
#include "../../src/core/types.h"
#include "../../src/core/w3e.h"
#include "../../src/core/doo.h"
#include "../../src/core/units_doo.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QApplication>

// ============================================================
// Helpers
// ============================================================
QString SampleTabPlugin::player_type_str(int type) {
    switch (type) {
        case 1: return "Human";
        case 2: return "Computer";
        case 3: return "Neutral";
        case 4: return "Rescuable";
        default: return QString::number(type);
    }
}

QString SampleTabPlugin::race_str(int race) {
    switch (race) {
        case 1: return "Human";
        case 2: return "Orc";
        case 3: return "Night Elf";
        case 4: return "Undead";
        case 5: return "Demon";
        case 6: return "Creeps";
        case 7: return "Other";
        default: return QString::number(race);
    }
}

QString SampleTabPlugin::flag_desc(int flags) {
    QStringList descs;
    if (flags & 0x0001) descs << "Hidden Minimap";
    if (flags & 0x0002) descs << "Modified Ally Priorities";
    if (flags & 0x0004) descs << "Melee Map";
    if (flags & 0x0008) descs << "Large Map Fix";
    if (flags & 0x0010) descs << "Masked Areas Partial Visible";
    if (flags & 0x0020) descs << "Fixed Player Settings";
    if (flags & 0x0040) descs << "Custom Forces";
    if (flags & 0x0080) descs << "Custom Techtree";
    if (flags & 0x0100) descs << "Custom Abilities";
    if (flags & 0x0200) descs << "Custom Upgrades";
    if (flags & 0x0400) descs << "Properties Opened";
    if (flags & 0x0800) descs << "Water Waves on Cliffs";
    if (flags & 0x1000) descs << "Water Waves on Rolling Shores";
    return descs.isEmpty() ? "None" : descs.join(", ");
}

QString SampleTabPlugin::format_file_size(int64_t bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1.%2 KB").arg(bytes / 1024).arg((bytes % 1024) / 103, 1, 10, QChar('0'));
    double mb = bytes / (1024.0 * 1024.0);
    return QString("%1 MB").arg(mb, 0, 'f', 2);
}

// ============================================================
// TRIGSTR resolution
// ============================================================
QString SampleTabPlugin::resolve(const std::string& text) const {
    // Check if the entire string is a TRIGSTR_XXX reference
    if (text.size() > 8 && text.substr(0, 8) == "TRIGSTR_") {
        char* end = nullptr;
        long key = std::strtol(text.c_str() + 8, &end, 10);
        if (*end == '\0') {
            auto it = wts_.find(static_cast<int>(key));
            if (it != wts_.end())
                return QString::fromStdString(it->second);
        }
    }
    return QString::fromStdString(text);
}

// ============================================================
// Tree helpers
// ============================================================
QTreeWidgetItem* SampleTabPlugin::add_category(const QString& title) {
    auto* item = new QTreeWidgetItem();
    item->setText(0, title);
    QFont f = item->font(0);
    f.setBold(true);
    f.setPointSize(f.pointSize() + 1);
    item->setFont(0, f);
    tree_->addTopLevelItem(item);
    return item;
}

QTreeWidgetItem* SampleTabPlugin::add_subcategory(QTreeWidgetItem* parent, const QString& title) {
    auto* item = new QTreeWidgetItem();
    item->setText(0, title);
    QFont f = item->font(0);
    f.setBold(true);
    item->setFont(0, f);
    parent->addChild(item);
    return item;
}

void SampleTabPlugin::add_item(QTreeWidgetItem* parent, const QString& key, const QString& value) {
    auto* item = new QTreeWidgetItem();
    item->setText(0, key + ":  " + value);
    parent->addChild(item);
}

// ============================================================
// Init
// ============================================================
bool SampleTabPlugin::init(PluginContext& ctx) {
    ctx_ = ctx;

    widget_ = new QWidget(ctx.parent_widget);
    auto* layout = new QVBoxLayout(widget_);

    tree_ = new QTreeWidget(widget_);
    tree_->setHeaderHidden(true);
    tree_->setAlternatingRowColors(true);
    tree_->setAnimated(true);
    tree_->setTextElideMode(Qt::ElideNone);
    tree_->setSelectionMode(QAbstractItemView::NoSelection);
    tree_->setFocusPolicy(Qt::NoFocus);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    // Show placeholder
    auto* placeholder = new QTreeWidgetItem(tree_);
    placeholder->setText(0, "No map loaded. Open a .w3x map to view its data.");
    placeholder->setForeground(0, QColor(140, 140, 140));

    layout->addWidget(tree_);
    return true;
}

// ============================================================
// Activate — read and display all map data
// ============================================================
void SampleTabPlugin::activate() {
    if (!tree_ || !ctx_.builder) return;
    populate_all();
}

void SampleTabPlugin::deactivate() {
    if (tree_)
        tree_->clear();
}

void SampleTabPlugin::destroy() {
    delete widget_;
    widget_ = nullptr;
    tree_ = nullptr;
}

// ============================================================
// Populate — read all data from builder into the tree
// ============================================================
void SampleTabPlugin::populate_all() {
    tree_->clear();

    // Pre-load WTS string table for TRIGSTR_XXX resolution
    wts_.clear();
    try { wts_ = ctx_.builder->read_wts(); } catch (...) {}

    populate_map_info();
    populate_terrain();
    populate_objects();
    populate_units();
    populate_doodads();
    populate_strings();
    populate_files();

    // Expand all top-level categories
    for (int i = 0; i < tree_->topLevelItemCount(); ++i)
        tree_->topLevelItem(i)->setExpanded(true);
}

// ============================================================
// Map Info (w3i)
// ============================================================
void SampleTabPlugin::populate_map_info() {
    try {
        auto info = ctx_.builder->read_w3i();
        auto* cat = add_category(QString::fromUtf8(
            "Map Info"));

        add_item(cat, "Name",        resolve(info.map_name));
        add_item(cat, "Author",      resolve(info.author));
        add_item(cat, "Description", resolve(info.description));
        add_item(cat, "Recommended Players", resolve(info.players_desc));
        add_item(cat, "Map Version", QString::number(info.map_version));
        add_item(cat, "Editor Version", QString::number(info.editor_version));
        add_item(cat, "Map Flags",   flag_desc(info.map_flags));
        add_item(cat, "Script Type", info.script_type == 0 ? "JASS" : "Lua");
        add_item(cat, "Dimensions",  QString("%1 \xC3\x97 %2").arg(info.map_width).arg(info.map_height));
        add_item(cat, "Tileset",     QChar(info.ground_type));

        // Loading screen
        if (!info.loading_screen_path.empty() || !info.loading_screen_text.empty()) {
            auto* ls = add_subcategory(cat, "Loading Screen");
            if (!info.loading_screen_path.empty())
                add_item(ls, "Custom Model", resolve(info.loading_screen_path));
            add_item(ls, "Text",    resolve(info.loading_screen_text));
            add_item(ls, "Title",   resolve(info.loading_screen_title));
            add_item(ls, "Subtitle", resolve(info.loading_screen_subtitle));
        }

        // Fog
        if (info.fog.type != 0) {
            auto* fg = add_subcategory(cat, "Fog");
            QStringList fog_types = {"None", "Linear", "Exponential"};
            int ft = info.fog.type;
            add_item(fg, "Type",  ft >= 1 && ft <= 2 ? fog_types[ft] : QString::number(ft));
            add_item(fg, "Start Z", QString::number(info.fog.start_z, 'f', 1));
            add_item(fg, "End Z",   QString::number(info.fog.end_z, 'f', 1));
            add_item(fg, "Density", QString::number(info.fog.density, 'f', 4));
            add_item(fg, "Color",   QString("RGBA(%1, %2, %3, %4)")
                .arg(info.fog.r).arg(info.fog.g).arg(info.fog.b).arg(info.fog.a));
        }

        // Weather
        if (!info.weather_id.empty()) {
            add_item(cat, "Weather", resolve(info.weather_id));
        }

        // Players
        if (!info.players.empty()) {
            auto* pl = add_subcategory(cat,
                QString("Players (%1)").arg(info.players.size()));
            for (size_t i = 0; i < info.players.size(); ++i) {
                auto& p = info.players[i];
                auto* pe = add_subcategory(pl,
                    QString("Player %1").arg(i + 1));
                add_item(pe, "Name",  resolve(p.name));
                add_item(pe, "Type",  player_type_str(p.type));
                add_item(pe, "Race",  race_str(p.race));
                add_item(pe, "Start Position",
                    QString("(%1, %2)").arg(p.start_x, 0, 'f', 1).arg(p.start_y, 0, 'f', 1));
            }
        }

        // Forces
        if (!info.forces.empty()) {
            auto* fr = add_subcategory(cat,
                QString("Forces (%1)").arg(info.forces.size()));
            for (size_t i = 0; i < info.forces.size(); ++i) {
                auto& f = info.forces[i];
                QStringList parts;
                if (f.flags & 0x01) parts << "Allied";
                if (f.flags & 0x02) parts << "Allied Victory";
                if (f.flags & 0x04) parts << "Shared Vision";
                if (f.flags & 0x10) parts << "Shared Units";
                if (f.flags & 0x20) parts << "Shared Control";
                auto* fe = add_subcategory(fr,
                    QString("Force %1: %2").arg(i + 1)
                        .arg(resolve(f.name)));
                add_item(fe, "Players",  QString::number(f.player_mask, 2));
                add_item(fe, "Flags",    parts.isEmpty() ? "None" : parts.join(", "));
            }
        }

        // Tech
        if (!info.tech_upgrades.empty()) {
            auto* tu = add_subcategory(cat,
                QString("Tech Upgrades (%1)").arg(info.tech_upgrades.size()));
            for (auto& t : info.tech_upgrades) {
                auto* te = add_subcategory(tu,
                    QString("Player %1").arg(t.player_id + 1));
                add_item(te, "ID",    QString::fromStdString(t.id.str()));
                add_item(te, "Level", QString::number(t.level));
            }
        }

        // Random groups
        if (!info.random_groups.empty()) {
            auto* rg = add_subcategory(cat,
                QString("Random Groups (%1)").arg(info.random_groups.size()));
            for (auto& g : info.random_groups) {
                auto* ge = add_subcategory(rg, QString::fromStdString(g.id.str()));
                add_item(ge, "Count", QString::number(g.count));
                add_item(ge, "Chance", QString::number(g.chance));
            }
        }

        // Random items
        if (!info.random_items.empty()) {
            auto* ri = add_subcategory(cat,
                QString("Random Item Tables (%1)").arg(info.random_items.size()));
            for (auto& t : info.random_items) {
                auto* re = add_subcategory(ri, QString::fromStdString(t.id.str()));
                add_item(re, "Items", QString::number(t.item_ids.size()));
            }
        }

    } catch (const std::exception& e) {
        add_item(add_category("Map Info"),
            "Error", QString::fromStdString(e.what()));
    }
}

// ============================================================
// Terrain (w3e)
// ============================================================
void SampleTabPlugin::populate_terrain() {
    try {
        auto terrain = ctx_.builder->read_terrain();
        auto* cat = add_category(QString::fromUtf8(
            "Terrain"));

        // Tileset name lookup
        static const char* kTilesetNames[256] = {};
        if (!kTilesetNames[0]) {
            kTilesetNames['A'] = "Ashenvale";
            kTilesetNames['B'] = "Barrens";
            kTilesetNames['C'] = "Felwood";
            kTilesetNames['D'] = "Dungeon";
            kTilesetNames['F'] = "Lordaeron Fall";
            kTilesetNames['G'] = "Underground";
            kTilesetNames['I'] = "Icecrown";
            kTilesetNames['J'] = "Dalaran Ruins";
            kTilesetNames['K'] = "Black Citadel";
            kTilesetNames['L'] = "Lordaeron Summer";
            kTilesetNames['N'] = "Northrend";
            kTilesetNames['O'] = "Outland";
            kTilesetNames['Q'] = "Village Fall";
            kTilesetNames['V'] = "Village";
            kTilesetNames['W'] = "Lordaeron Winter";
            kTilesetNames['X'] = "Dalaran";
            kTilesetNames['Y'] = "Cityscape";
            kTilesetNames['Z'] = "Sunken Ruins";
        }

        char ts = terrain.tileset;
        const char* ts_name = kTilesetNames[(unsigned char)ts];
        QString tileset_str = ts_name
            ? QString("%1 (%2)").arg(ts_name).arg(ts)
            : QString::number(ts);

        add_item(cat, "Tileset", tileset_str);
        add_item(cat, "Custom Tileset", terrain.custom_tileset ? "Yes" : "No");
        add_item(cat, "Map Tiles",
            QString("%1 \xC3\x97 %2").arg(terrain.tile_width - 1).arg(terrain.tile_height - 1));
        add_item(cat, "Tile Points", QString("%1").arg(terrain.tilepoints.size()));

        // Ground tiles
        if (!terrain.ground_tiles.empty()) {
            QStringList ids;
            for (auto& t : terrain.ground_tiles)
                ids << QString::fromStdString(t);
            add_item(cat, "Ground Tiles", ids.join(", "));
        }

        // Cliff tiles
        if (!terrain.cliff_tiles.empty()) {
            QStringList ids;
            for (auto& t : terrain.cliff_tiles)
                ids << QString::fromStdString(t);
            add_item(cat, "Cliff Tiles", ids.join(", "));
        }

        // Water summary — count tiles with water
        int water_count = 0, ramp_count = 0, edge_count = 0;
        for (auto& tp : terrain.tilepoints) {
            if (tp.flags & 0x04) water_count++;
            if (tp.flags & 0x01) ramp_count++;
            if (tp.map_edge)     edge_count++;
        }
        if (water_count > 0)
            add_item(cat, "Water Tiles", QString::number(water_count));
        if (ramp_count > 0)
            add_item(cat, "Ramp Tiles", QString::number(ramp_count));
        if (edge_count > 0)
            add_item(cat, "Map Edge Tiles", QString::number(edge_count));

    } catch (const std::exception& e) {
        // Terrain file missing — not an error
    }
}

// ============================================================
// Objects (w3u / w3t / w3b / w3d / w3a / w3h / w3q)
// ============================================================
void SampleTabPlugin::populate_objects() {
    // Object file names and their display labels
    static const struct { const char* file; const char* label; } kObjectFiles[] = {
        {"war3map.w3u", "Units"},
        {"war3map.w3t", "Items"},
        {"war3map.w3b", "Destructables"},
        {"war3map.w3d", "Doodads"},
        {"war3map.w3a", "Abilities"},
        {"war3map.w3h", "Buffs"},
        {"war3map.w3q", "Upgrades"},
    };

    auto* cat = add_category(QString::fromUtf8(
        "Objects"));

    bool any = false;
    for (auto& kf : kObjectFiles) {
        try {
            auto obj = ctx_.builder->read_object(kf.file);
            auto* oc = add_subcategory(cat,
                QString("%1 (%2)").arg(kf.label).arg(kf.file));

            int orig_mods = 0, custom_mods = 0;
            for (auto& e : obj.original_objects)
                orig_mods += (int)e.mods.size();
            for (auto& e : obj.custom_objects)
                custom_mods += (int)e.mods.size();

            add_item(oc, "Original Objects",
                QString("%1 (%2 modifications)")
                    .arg(obj.original_objects.size()).arg(orig_mods));
            add_item(oc, "Custom Objects",
                QString("%1 (%2 modifications)")
                    .arg(obj.custom_objects.size()).arg(custom_mods));

            // List custom object IDs with names
            if (!obj.custom_objects.empty()) {
                auto* co = add_subcategory(oc, "Custom Objects Detail");
                int limit = 0;
                for (auto& e : obj.custom_objects) {
                    if (++limit > 100) {
                        add_item(co, "...",
                            QString("and %1 more").arg(obj.custom_objects.size() - 100));
                        break;
                    }
                    QString obj_id = QString::fromStdString(e.new_id.str());
                    QString name;
                    if (ctx_.meta_db) {
                        auto* n = ctx_.meta_db->find_object_name(e.new_id.str());
                        if (n) name = QString::fromStdString(*n);
                    }
                    QString label = name.isEmpty()
                        ? obj_id
                        : QString("%1 (%2)").arg(name).arg(obj_id);
                    add_item(co, label, QString("%1 mods").arg(e.mods.size()));
                }
            }

            any = true;
        } catch (const std::exception&) {
            // File not present — skip
        }
    }

    if (!any)
        add_item(cat, "Info", "No object files found in this map.");
}

// ============================================================
// Units (war3mapUnits.doo)
// ============================================================
void SampleTabPlugin::populate_units() {
    try {
        auto units = ctx_.builder->read_placed_units();
        auto* cat = add_category(QString::fromUtf8(
            "Units"));

        add_item(cat, "Total Units", QString::number(units.units.size()));

        if (units.units.empty())
            return;

        // Count units per player
        std::map<int, int> per_player;
        std::map<std::string, int> per_type;
        int hero_count = 0;
        for (auto& u : units.units) {
            per_player[u.owner]++;
            per_type[u.type_id]++;
            if (u.hero_level > 0) hero_count++;
        }

        if (hero_count > 0)
            add_item(cat, "Heroes", QString::number(hero_count));

        // By player
        auto* bp = add_subcategory(cat,
            QString("By Player (%1)").arg(per_player.size()));
        for (auto& [owner, count] : per_player) {
            QString name = owner == 16 ? "Neutral Passive"
                                       : QString("Player %1").arg(owner + 1);
            add_item(bp, name, QString::number(count));
        }

        // Top unit types
        if (per_type.size() > 1) {
            auto* bt = add_subcategory(cat,
                QString("By Type (%1)").arg(per_type.size()));

            // Sort by count descending
            std::vector<std::pair<std::string, int>> sorted(per_type.begin(), per_type.end());
            std::sort(sorted.begin(), sorted.end(),
                [](auto& a, auto& b) { return a.second > b.second; });

            int limit = 0;
            for (auto& [id, count] : sorted) {
                if (++limit > 50) {
                    add_item(bt, "...",
                        QString("and %1 more").arg(per_type.size() - 50));
                    break;
                }
                QString name;
                if (ctx_.meta_db) {
                    auto* n = ctx_.meta_db->find_object_name(id);
                    if (n) name = QString::fromStdString(*n);
                }
                QString label = name.isEmpty()
                    ? QString::fromStdString(id)
                    : QString("%1 (%2)").arg(name).arg(QString::fromStdString(id));
                add_item(bt, label, QString::number(count));
            }
        }

    } catch (const std::exception&) {
        // No unit placement data
    }
}

// ============================================================
// Doodads (war3map.doo)
// ============================================================
void SampleTabPlugin::populate_doodads() {
    try {
        auto doodads = ctx_.builder->read_placed_doodads();
        auto* cat = add_category(QString::fromUtf8(
            "Doodads"));

        add_item(cat, "Total Doodads", QString::number(doodads.doodads.size()));
        add_item(cat, "Special Doodads", QString::number(doodads.special.size()));

        if (doodads.doodads.empty())
            return;

        // Count by type
        std::map<std::string, int> per_type;
        for (auto& d : doodads.doodads)
            per_type[d.type_id]++;

        auto* bt = add_subcategory(cat,
            QString("By Type (%1)").arg(per_type.size()));

        std::vector<std::pair<std::string, int>> sorted(per_type.begin(), per_type.end());
        std::sort(sorted.begin(), sorted.end(),
            [](auto& a, auto& b) { return a.second > b.second; });

        int limit = 0;
        for (auto& [id, count] : sorted) {
            if (++limit > 50) {
                add_item(bt, "...",
                    QString("and %1 more").arg(per_type.size() - 50));
                break;
            }
            QString name;
            if (ctx_.meta_db) {
                auto* n = ctx_.meta_db->find_object_name(id);
                if (n) name = QString::fromStdString(*n);
            }
            QString label = name.isEmpty()
                ? QString::fromStdString(id)
                : QString("%1 (%2)").arg(name).arg(QString::fromStdString(id));
            add_item(bt, label, QString::number(count));
        }

    } catch (const std::exception&) {
        // No doodad data
    }
}

// ============================================================
// Strings (war3map.wts)
// ============================================================
void SampleTabPlugin::populate_strings() {
    auto* cat = add_category("Strings");

    if (wts_.empty()) {
        add_item(cat, "Info", "No string table (war3map.wts) found.");
        return;
    }

    add_item(cat, "Total Entries", QString::number(wts_.size()));

    // Show sample entries
    auto* sample = add_subcategory(cat,
        QString("Sample Entries (first %1)").arg(std::min(wts_.size(), size_t{20})));
    int count = 0;
    for (auto& [key, val] : wts_) {
        if (++count > 20) break;
        QString display = QString::fromStdString(val);
        if (display.length() > 80)
            display = display.left(80) + "...";
        add_item(sample, QString("TRIGSTR_%1").arg(key), display);
    }
}

// ============================================================
// Files (MPQ file listing)
// ============================================================
void SampleTabPlugin::populate_files() {
    try {
        auto files = ctx_.builder->list_files();
        auto* cat = add_category(QString::fromUtf8(
            "Files (%1)").arg(files.size()));

        // Sort and categorize
        std::map<std::string, int> ext_counts;
        int64_t total_size = 0;
        for (auto& name : files) {
            auto ext_pos = name.rfind('.');
            std::string ext = (ext_pos != std::string::npos)
                ? name.substr(ext_pos) : "(none)";
            ext_counts[ext]++;

            try {
                total_size += ctx_.builder->file_size(name);
            } catch (...) {}
        }

        add_item(cat, "Total Size", format_file_size(total_size));

        // File type summary
        auto* summary = add_subcategory(cat, "By Extension");
        for (auto& [ext, count] : ext_counts) {
            add_item(summary, QString::fromStdString(ext), QString::number(count));
        }

        // Full file list
        auto* fl = add_subcategory(cat, "All Files");
        for (auto& name : files) {
            QString size_str;
            try {
                int64_t sz = ctx_.builder->file_size(name);
                size_str = format_file_size(sz);
            } catch (...) {
                size_str = "?";
            }
            add_item(fl, QString::fromStdString(name), size_str);
        }

    } catch (const std::exception&) {
        // No files
    }
}

// ============================================================
// DLL factory
// ============================================================
extern "C" __declspec(dllexport) IEditorPlugin* w3x_plugin_create() {
    return new SampleTabPlugin();
}
