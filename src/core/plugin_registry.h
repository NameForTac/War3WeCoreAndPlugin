#pragma once

#include "plugin.h"
#include <vector>
#include <memory>
#include <string>

// ============================================================
// PluginRegistry — singleton that manages DLL plugin lifecycle
// ============================================================
class PluginRegistry {
public:
    static PluginRegistry& instance();

    // Register a compiled-in plugin instance (no DLL)
    void register_plugin(std::unique_ptr<IEditorPlugin> plugin);

    // Load all DLL plugins from a directory
    // Returns the number of plugins loaded successfully.
    int load_directory(const QString& dir_path);

    // Initialize all loaded plugins (calls init on each)
    void init_all(PluginContext& ctx);

    // Activate/deactivate all plugins (called on map load/unload)
    void activate_all();
    void deactivate_all();

    // Sync all NeedsSavable plugins to the builder
    void sync_all();

    // Destroy and unload all plugins
    void destroy_all();

    // Access
    size_t count() const;
    IEditorPlugin* get(size_t index) const;
    IEditorPlugin* find(const QString& name) const;

    // Reload all plugins (unload + load)
    int reload_all(const QString& dir_path);

private:
    PluginRegistry() = default;
    ~PluginRegistry();
    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;

    void unload_all();

    struct Entry {
        bool              is_builtin = false;
        std::string       dll_path;    // full path to loaded DLL
        void*             handle;      // HMODULE / void*
        PluginFactoryFunc factory;     // cached function pointer
        std::unique_ptr<IEditorPlugin> instance;
    };
    std::vector<Entry> entries_;
};
