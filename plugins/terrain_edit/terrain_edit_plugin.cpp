#include "terrain_edit_plugin.h"
#include "terrain_edit_window.h"
#include "../../src/core/map_builder.h"

#include <QCoreApplication>
#include <QTranslator>
#include <QLocale>

bool TerrainEditPlugin::init(PluginContext& ctx) {
    builder_ = ctx.builder;

    // Load plugin-level translations BEFORE creating any UI,
    // so tr() calls in the window/widget constructors pick up the right locale.
    translator_ = new QTranslator(this);
    QString qmDir = QCoreApplication::applicationDirPath() + "/plugins/";
    QString lang = QLocale::system().name();
    if (translator_->load("terrain_edit_" + lang, qmDir)
        || translator_->load("terrain_edit_" + lang.left(2), qmDir)
        || translator_->load("terrain_edit_zh_CN", qmDir)) {
        QCoreApplication::installTranslator(translator_);
    } else {
        delete translator_;
        translator_ = nullptr;
    }

    window_ = new TerrainEditWindow(ctx.parent_widget);
    open_action_ = new QAction(tr("Terrain Editor..."), nullptr);
    QObject::connect(open_action_, &QAction::triggered, [this]() {
        if (window_) {
            window_->show();
            window_->raise();
            window_->activateWindow();
        }
    });

    return true;
}

void TerrainEditPlugin::activate() {
    if (window_ && builder_) {
        window_->onMapLoaded(builder_);
    }
}

void TerrainEditPlugin::deactivate() {
    if (window_) {
        window_->onMapClosed();
    }
}

void TerrainEditPlugin::destroy() {
    delete open_action_;
    open_action_ = nullptr;

    if (window_->isVisible())
        window_->close();
    delete window_;
    window_ = nullptr;
    builder_ = nullptr;

    if (translator_) {
        QCoreApplication::removeTranslator(translator_);
        delete translator_;
        translator_ = nullptr;
    }
}

void TerrainEditPlugin::sync_to_builder() {
    if (window_ && builder_ && window_->isModified()) {
        window_->syncToBuilder(builder_);
    }
}

QList<QAction*> TerrainEditPlugin::menu_actions() {
    if (open_action_)
        return { open_action_ };
    return {};
}

// DLL factory
extern "C" __declspec(dllexport) IEditorPlugin* w3x_plugin_create() {
    return new TerrainEditPlugin();
}
