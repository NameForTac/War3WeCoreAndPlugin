#include "plugin_registry.h"
#include <QDir>
#include <QDebug>
#include <cassert>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

// ============================================================
// Platform helpers for DLL loading
// ============================================================
static void* load_library(const std::string& path) {
#ifdef _WIN32
    return static_cast<void*>(LoadLibraryA(path.c_str()));
#else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

static void* find_symbol(void* handle, const char* symbol) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(
        static_cast<HMODULE>(handle), symbol));
#else
    return dlsym(handle, symbol);
#endif
}

static void free_library(void* handle) {
    if (!handle) return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

// ============================================================
// PluginRegistry implementation
// ============================================================
PluginRegistry& PluginRegistry::instance() {
    static PluginRegistry reg;
    return reg;
}

PluginRegistry::~PluginRegistry() {
    destroy_all();
    unload_all();
}

void PluginRegistry::register_plugin(std::unique_ptr<IEditorPlugin> plugin) {
    Entry entry;
    entry.is_builtin = true;
    entry.handle = nullptr;
    entry.instance = std::move(plugin);
    entries_.push_back(std::move(entry));
}

void PluginRegistry::sync_all() {
    for (auto& entry : entries_) {
        if (entry.instance &&
            has_cap(entry.instance->capabilities(), PluginCapability::NeedsSavable))
            entry.instance->sync_to_builder();
    }
}

int PluginRegistry::load_directory(const QString& dir_path) {
    QDir dir(dir_path);
    if (!dir.exists())
        return 0;

    int loaded = 0;
    QStringList filters;
#ifdef _WIN32
    filters << "*.dll";
#else
    filters << "*.so";
#endif

    for (const auto& fi : dir.entryInfoList(filters, QDir::Files)) {
        std::string path = fi.absoluteFilePath().toStdString();

        void* handle = load_library(path);
        if (!handle) {
            qWarning() << "PluginRegistry: failed to load" << fi.fileName();
            continue;
        }

        auto factory = reinterpret_cast<PluginFactoryFunc>(
            find_symbol(handle, "w3x_plugin_create"));
        if (!factory) {
            qWarning() << "PluginRegistry: no w3x_plugin_create in" << fi.fileName();
            free_library(handle);
            continue;
        }

        Entry entry;
        entry.dll_path = path;
        entry.handle   = handle;
        entry.factory  = factory;
        entries_.push_back(std::move(entry));
        loaded++;
    }

    return loaded;
}

void PluginRegistry::init_all(PluginContext& ctx) {
    for (auto& entry : entries_) {
        if (entry.instance)
            continue; // already initialized
        if (!entry.factory)
            continue; // compiled-in with no factory (shouldn't happen after register_plugin)
        auto* plugin = entry.factory();
        if (plugin && plugin->init(ctx)) {
            entry.instance.reset(plugin);
        } else {
            delete plugin;
            qWarning() << "PluginRegistry: init failed for" << QString::fromStdString(entry.dll_path);
        }
    }
}

void PluginRegistry::activate_all() {
    for (auto& entry : entries_) {
        if (entry.instance)
            entry.instance->activate();
    }
}

void PluginRegistry::deactivate_all() {
    for (auto& entry : entries_) {
        if (entry.instance)
            entry.instance->deactivate();
    }
}

void PluginRegistry::destroy_all() {
    for (auto& entry : entries_) {
        if (entry.instance) {
            entry.instance->destroy();
            entry.instance.reset();
        }
    }
}

void PluginRegistry::unload_all() {
    for (auto& entry : entries_) {
        if (entry.handle) {
            free_library(entry.handle);
            entry.handle = nullptr;
        }
    }
    entries_.clear();
}

size_t PluginRegistry::count() const {
    return entries_.size();
}

IEditorPlugin* PluginRegistry::get(size_t index) const {
    if (index >= entries_.size()) return nullptr;
    return entries_[index].instance.get();
}

IEditorPlugin* PluginRegistry::find(const QString& name) const {
    for (auto& entry : entries_) {
        if (entry.instance && entry.instance->name() == name)
            return entry.instance.get();
    }
    return nullptr;
}

int PluginRegistry::reload_all(const QString& dir_path) {
    // Deactivate compiled-in, destroy + remove DLL entries
    for (auto it = entries_.begin(); it != entries_.end(); ) {
        if (it->is_builtin) {
            if (it->instance)
                it->instance->deactivate();
            ++it;
        } else {
            if (it->instance) {
                it->instance->destroy();
                it->instance.reset();
            }
            if (it->handle) {
                free_library(it->handle);
                it->handle = nullptr;
            }
            it = entries_.erase(it);
        }
    }
    return load_directory(dir_path);
}
