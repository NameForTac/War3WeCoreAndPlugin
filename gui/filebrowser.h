#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>

#include "../src/core/plugin.h"

class MapBuilder;

class FileBrowser : public QWidget {
    Q_OBJECT
public:
    explicit FileBrowser(QWidget* parent = nullptr);

    void load(MapBuilder* builder);
    void clear();

signals:
    void contentChanged();

private slots:
    void on_extract_selected();
    void on_extract_all();
    void on_replace();
    void on_remove();

private:
    void build_ui();
    void populate_tree(const std::vector<std::string>& files);

    QTreeWidget* tree_ = nullptr;
    QPushButton* extract_btn_ = nullptr;
    QPushButton* extract_all_btn_ = nullptr;
    QPushButton* replace_btn_ = nullptr;
    QPushButton* remove_btn_ = nullptr;

    MapBuilder* builder_ = nullptr;
};

// ============================================================
// FileBrowserPlugin — IEditorPlugin wrapper for FileBrowser
// ============================================================
class FileBrowserPlugin : public QObject, public IEditorPlugin {
    Q_OBJECT
public:
    QString name() const override { return tr("File Browser"); }
    QString version() const override { return "1.0"; }
    PluginCapability capabilities() const override {
        return PluginCapability::ProvidesTab;
    }

    bool init(PluginContext& ctx) override;
    void activate() override;
    void deactivate() override;
    void destroy() override;

    QWidget* tab_widget() override { return browser_; }
    QString tab_title() override { return tr("File Browser"); }

signals:
    void contentChanged();

private:
    FileBrowser* browser_ = nullptr;
    MapBuilder* builder_ = nullptr;
};
