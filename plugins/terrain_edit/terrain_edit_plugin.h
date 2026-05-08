#pragma once

#include "../../src/core/plugin.h"
#include <QObject>
#include <QAction>
#include <QTranslator>

class TerrainEditWindow;
class MapBuilder;

class TerrainEditPlugin : public QObject, public IEditorPlugin {
    Q_OBJECT
public:
    QString name() const override { return tr("Terrain Editor"); }
    QString version() const override { return "1.0"; }
    PluginCapability capabilities() const override {
        return PluginCapability::ProvidesMenu | PluginCapability::NeedsSavable;
    }

    bool init(PluginContext& ctx) override;
    void activate() override;
    void deactivate() override;
    void destroy() override;
    void sync_to_builder() override;

    QList<QAction*> menu_actions() override;

private:
    TerrainEditWindow* window_ = nullptr;
    MapBuilder* builder_ = nullptr;
    QAction* open_action_ = nullptr;
    QTranslator* translator_ = nullptr;
};
