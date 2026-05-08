#pragma once

#include <QMainWindow>
#include <memory>

#include "../src/core/map_builder.h"
#include "../src/core/plugin_registry.h"
#include "../src/core/metadata.h"
#include "../src/core/wc3_manager.h"

class QTabWidget;
class QToolBar;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_open();
    void on_save();
    void on_save_as();
    void on_extract_all();
    void on_settings();
    void on_about();
    void on_content_changed() { modified_ = true; }

private:
    void create_actions();
    void create_menus();
    void create_toolbar();
    void create_widgets();
    void load_meta_data();
    void update_title();
    void load_all();
    void save_settings();
    void closeEvent(QCloseEvent* event) override;

    bool modified_ = false;
    QTabWidget* tabs_ = nullptr;

    std::unique_ptr<MapBuilder> builder_;
    BuildSettings settings_;
    QString current_path_;
    QString plugin_dir_;
    QString wc3_data_dir_;

    MetaDataDB meta_db_;
    Wc3Manager wc3_manager_;

    QAction* open_action_ = nullptr;
    QAction* save_action_ = nullptr;
    QAction* save_as_action_ = nullptr;
    QAction* extract_all_action_ = nullptr;
    QAction* settings_action_ = nullptr;
    QAction* about_action_ = nullptr;
    QAction* exit_action_ = nullptr;

    QToolBar* main_toolbar_ = nullptr;
    QLabel* status_label_ = nullptr;
};
