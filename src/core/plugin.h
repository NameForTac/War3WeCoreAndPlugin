#pragma once

#include <cstdint>
#include <QString>
#include <QWidget>
#include <QList>
#include <QAction>

class MapBuilder;
class MetaDataDB;
class Wc3Manager;

// ============================================================
// Plugin capabilities bitmask
// ============================================================
enum class PluginCapability : uint32_t {
    None         = 0,
    ProvidesTab  = 1 << 0,  // adds a tab to MainWindow
    ProvidesMenu = 1 << 1,  // adds menu items
    ProvidesTool = 1 << 2,  // adds toolbar actions
    NeedsSavable = 1 << 3,  // participates in save pipeline
};

inline PluginCapability operator|(PluginCapability a, PluginCapability b) {
    return static_cast<PluginCapability>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool has_cap(PluginCapability caps, PluginCapability flag) {
    return (static_cast<uint32_t>(caps) & static_cast<uint32_t>(flag)) != 0;
}

// ============================================================
// Context passed to plugins during initialization
// ============================================================
struct PluginContext {
    MapBuilder* builder = nullptr;
    MetaDataDB*  meta_db = nullptr;
    QWidget*    parent_widget = nullptr;
    // WC3 installation directory (raw path — use for MetaDataDB etc.)
    std::string wc3_data_dir;
    // WC3 resource manager — opens War3.mpq/War3x.mpq for reading game assets.
    Wc3Manager* wc3 = nullptr;
};

// ============================================================
// IEditorPlugin — base interface for all editor plugins
// ============================================================
class IEditorPlugin {
public:
    virtual ~IEditorPlugin() = default;

    // --- Identity ---
    virtual QString     name() const = 0;
    virtual QString     version() const = 0;
    virtual PluginCapability capabilities() const = 0;

    // --- Lifecycle ---
    // init() is called once after the plugin is loaded.
    // Return false to indicate the plugin should be unloaded.
    virtual bool        init(PluginContext& ctx) = 0;
    // activate/deactivate are called each time a map is loaded/unloaded
    virtual void        activate() = 0;
    virtual void        deactivate() = 0;
    virtual void        destroy() = 0;

    // --- Tab integration ---
    virtual QWidget*    tab_widget() { return nullptr; }
    virtual QString     tab_title()  { return QString(); }

    // --- Save pipeline ---
    virtual void        sync_to_builder() {}

    // --- Menu / Toolbar ---
    virtual QList<QAction*> menu_actions()    { return {}; }
    virtual QList<QAction*> toolbar_actions() { return {}; }
};

// ============================================================
// DLL plugin factory function type
// Each plugin DLL exports this C-ABI function.
// ============================================================
extern "C" typedef IEditorPlugin* (*PluginFactoryFunc)();
