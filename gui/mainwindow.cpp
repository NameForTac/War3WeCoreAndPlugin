#include "mainwindow.h"
#include "mapinfopage.h"
#include "filebrowser.h"
#include "objecteditor.h"
#include "terraineditor.h"
#include "placementeditor.h"
#include "settingsdialog.h"

#include <QTabWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QStyle>
#include <QApplication>
#include <direct.h>

static void mkdir_recursive(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        char c = path[i];
        if (c == '/' || c == '\\') {
            _mkdir(cur.c_str());
        }
        cur += c;
    }
    _mkdir(cur.c_str());
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , builder_(std::make_unique<MapBuilder>())
{
    setWindowTitle("w3x-packer");

    QSettings qs;
    settings_.mode = static_cast<OutputMode>(
        qs.value("BuildSettings/Mode", static_cast<int>(OutputMode::OBJ)).toInt());
    settings_.remove_listfile = qs.value("BuildSettings/RemoveListfile", true).toBool();
    settings_.remove_attributes = qs.value("BuildSettings/RemoveAttributes", true).toBool();
    plugin_dir_ = qs.value("Plugins/Directory", QApplication::applicationDirPath() + "/plugins/").toString();
    wc3_data_dir_ = qs.value("WC3Data/Directory", "").toString();

    create_actions();
    create_menus();
    create_toolbar();
    create_widgets();

    // Load WC3 metadata (for ObjectEditor autocomplete)
    load_meta_data();

    // Restore window geometry
    auto geom = qs.value("MainWindow/Geometry").toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    else
        resize(1024, 700);
    auto state = qs.value("MainWindow/State").toByteArray();
    if (!state.isEmpty())
        restoreState(state);

    statusBar()->showMessage(tr("Ready"));
}

MainWindow::~MainWindow() {
    PluginRegistry::instance().deactivate_all();
    PluginRegistry::instance().destroy_all();
    save_settings();
}

void MainWindow::create_actions() {
    open_action_ = new QAction(tr("&Open..."), this);
    open_action_->setShortcut(QKeySequence::Open);
    open_action_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    connect(open_action_, &QAction::triggered, this, &MainWindow::on_open);

    save_action_ = new QAction(tr("&Save"), this);
    save_action_->setShortcut(QKeySequence::Save);
    save_action_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    save_action_->setEnabled(false);
    connect(save_action_, &QAction::triggered, this, &MainWindow::on_save);

    save_as_action_ = new QAction(tr("Save &As..."), this);
    save_as_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    save_as_action_->setEnabled(false);
    connect(save_as_action_, &QAction::triggered, this, &MainWindow::on_save_as);

    extract_all_action_ = new QAction(tr("&Extract All..."), this);
    extract_all_action_->setEnabled(false);
    connect(extract_all_action_, &QAction::triggered, this, &MainWindow::on_extract_all);

    settings_action_ = new QAction(tr("&Settings..."), this);
    connect(settings_action_, &QAction::triggered, this, &MainWindow::on_settings);

    about_action_ = new QAction(tr("&About"), this);
    connect(about_action_, &QAction::triggered, this, &MainWindow::on_about);

    exit_action_ = new QAction(tr("E&xit"), this);
    exit_action_->setShortcut(QKeySequence::Quit);
    connect(exit_action_, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::create_menus() {
    auto* file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->addAction(open_action_);
    file_menu->addAction(save_action_);
    file_menu->addAction(save_as_action_);
    file_menu->addSeparator();
    file_menu->addAction(extract_all_action_);
    file_menu->addSeparator();
    file_menu->addAction(exit_action_);

    auto* tools_menu = menuBar()->addMenu(tr("&Tools"));
    tools_menu->addAction(settings_action_);

    // Plugin menu (populated later by init_plugins)
    menuBar()->addMenu(tr("&Plugins"));

    auto* help_menu = menuBar()->addMenu(tr("&Help"));
    help_menu->addAction(about_action_);
}

void MainWindow::create_toolbar() {
    main_toolbar_ = addToolBar(tr("Main"));
    main_toolbar_->setObjectName("MainToolBar");
    main_toolbar_->setMovable(false);
    main_toolbar_->addAction(open_action_);
    main_toolbar_->addAction(save_action_);
    main_toolbar_->addAction(extract_all_action_);
    main_toolbar_->addSeparator();
    main_toolbar_->addAction(settings_action_);
}

void MainWindow::create_widgets() {
    tabs_ = new QTabWidget(this);
    setCentralWidget(tabs_);

    // Register compiled-in editor plugins
    auto& reg = PluginRegistry::instance();
    reg.register_plugin(std::make_unique<MapInfoPlugin>());
    reg.register_plugin(std::make_unique<FileBrowserPlugin>());
    reg.register_plugin(std::make_unique<ObjectEditorPlugin>(&meta_db_));
    reg.register_plugin(std::make_unique<TerrainEditorPlugin>());
    reg.register_plugin(std::make_unique<PlacementEditorPlugin>());

    // Load DLL plugins
    reg.load_directory(plugin_dir_);

    // Init all plugins (compiled-in + DLL)
    PluginContext ctx;
    ctx.builder = builder_.get();
    ctx.meta_db = &meta_db_;
    ctx.parent_widget = this;
    reg.init_all(ctx);

    // Add tabs for all plugins with ProvidesTab capability
    for (size_t i = 0; i < reg.count(); ++i) {
        auto* plugin = reg.get(i);
        if (!plugin) continue;
        if (has_cap(plugin->capabilities(), PluginCapability::ProvidesTab)) {
            auto* w = plugin->tab_widget();
            auto title = plugin->tab_title();
            if (w && !title.isEmpty()) {
                int idx = tabs_->addTab(w, title);
                tabs_->setTabEnabled(idx, false);
                // Track modified via widget's contentChanged signal
                if (auto* obj = qobject_cast<QObject*>(w)) {
                    // Only connect if the widget actually declares contentChanged
                    if (obj->metaObject()->indexOfSignal("contentChanged()") >= 0)
                        connect(obj, SIGNAL(contentChanged()), this, SLOT(on_content_changed()));
                }
            }
        }
    }

    // Add plugin menu actions
    auto* plugin_menu = menuBar()->actions().at(3); // File, Tools, Plugins, Help
    auto* pm = plugin_menu->menu();
    if (pm) {
        for (size_t i = 0; i < reg.count(); ++i) {
            auto* plugin = reg.get(i);
            if (!plugin) continue;
            if (has_cap(plugin->capabilities(), PluginCapability::ProvidesMenu)) {
                for (auto* a : plugin->menu_actions())
                    pm->addAction(a);
            }
        }
    }

    // Add plugin toolbar actions
    for (size_t i = 0; i < reg.count(); ++i) {
        auto* plugin = reg.get(i);
        if (!plugin) continue;
        if (has_cap(plugin->capabilities(), PluginCapability::ProvidesTool)) {
            for (auto* a : plugin->toolbar_actions())
                main_toolbar_->addAction(a);
        }
    }
}

void MainWindow::load_meta_data() {
    if (!wc3_data_dir_.isEmpty()) {
        if (!meta_db_.load_data_dir(wc3_data_dir_.toStdString())) {
            statusBar()->showMessage(tr("Failed to load WC3 metadata from: %1").arg(wc3_data_dir_));
        }
    }
}

void MainWindow::update_title() {
    QString title = "w3x-packer";
    if (!current_path_.isEmpty())
        title += " - " + current_path_;
    setWindowTitle(title);
}

void MainWindow::load_all() {
    if (!builder_ || current_path_.isEmpty())
        return;

    try {
        // Activate all plugins — each reads what it needs from the builder
        PluginRegistry::instance().activate_all();

        // Enable all tabs
        for (int i = 0; i < tabs_->count(); ++i)
            tabs_->setTabEnabled(i, true);

        modified_ = false;
        save_action_->setEnabled(true);
        save_as_action_->setEnabled(true);
        extract_all_action_->setEnabled(true);
        statusBar()->showMessage(tr("Loaded: %1").arg(current_path_));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Error"),
            tr("Failed to load map: %1").arg(e.what()));
    }
}

void MainWindow::on_open() {
    if (modified_) {
        auto ret = QMessageBox::warning(this, tr("Unsaved Changes"),
            tr("Do you want to save changes before opening another map?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (ret == QMessageBox::Save) {
            on_save();
        } else if (ret == QMessageBox::Cancel) {
            return;
        }
    }

    // Deactivate plugins before loading new map
    PluginRegistry::instance().deactivate_all();

    QString path = QFileDialog::getOpenFileName(
        this, tr("Open W3X Map"), QString(),
        tr("Warcraft III Maps (*.w3x *.w3m);;All Files (*)"));
    if (path.isEmpty()) return;

    if (!builder_->open_source(path.toLocal8Bit().toStdString())) {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to open: %1").arg(path));
        return;
    }

    current_path_ = path;
    update_title();
    load_all();
}

void MainWindow::on_save() {
    if (current_path_.isEmpty()) {
        on_save_as();
        return;
    }

    // Sync all NeedsSavable plugins to the builder
    PluginRegistry::instance().sync_all();

    BuildSettings s = settings_;

    if (builder_->save(current_path_.toLocal8Bit().toStdString(), s)) {
        modified_ = false;
        statusBar()->showMessage(tr("Saved successfully"));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save map"));
    }
}

void MainWindow::on_save_as() {
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save W3X Map As"), QString(),
        tr("Warcraft III Maps (*.w3x);;All Files (*)"));
    if (path.isEmpty()) return;

    // Sync all NeedsSavable plugins to the builder
    PluginRegistry::instance().sync_all();

    BuildSettings s = settings_;

    if (builder_->save(path.toLocal8Bit().toStdString(), s)) {
        modified_ = false;
        current_path_ = path;
        update_title();
        statusBar()->showMessage(tr("Saved as: %1").arg(path));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save map"));
    }
}

void MainWindow::on_extract_all() {
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Extract to Directory"));
    if (dir.isEmpty()) return;

    auto files = builder_->list_files();
    int count = 0;
    for (auto& name : files) {
        auto data = builder_->read_raw(name);
        std::string out_path = dir.toLocal8Bit().toStdString() + "/" + name;
        auto slash = out_path.find_last_of('/');
        if (slash != std::string::npos) {
            mkdir_recursive(out_path.substr(0, slash));
        }
        FILE* f = fopen(out_path.c_str(), "wb");
        if (f) {
            fwrite(data.data(), 1, data.size(), f);
            fclose(f);
            count++;
        }
    }
    statusBar()->showMessage(tr("Extracted %1 files").arg(count));
}

void MainWindow::on_settings() {
    SettingsDialog dlg(this);
    dlg.set_settings(settings_);
    dlg.set_plugin_directory(plugin_dir_);
    dlg.set_wc3_data_directory(wc3_data_dir_);

    if (dlg.exec() == QDialog::Accepted) {
        settings_ = dlg.settings();
        QString new_plugin_dir = dlg.plugin_directory();
        QString new_wc3_dir = dlg.wc3_data_directory();

        // Reload WC3 metadata if directory changed
        if (new_wc3_dir != wc3_data_dir_) {
            wc3_data_dir_ = new_wc3_dir;
            meta_db_ = MetaDataDB{}; // reset
            if (!wc3_data_dir_.isEmpty()) {
                meta_db_.load_data_dir(wc3_data_dir_.toStdString());
            }
        }

        // Reload DLL plugins if the directory changed
        if (new_plugin_dir != plugin_dir_) {
            plugin_dir_ = new_plugin_dir;

            // Remove existing DLL plugin tabs (compiled-in tabs remain in registry)
            auto& reg = PluginRegistry::instance();
            for (size_t i = 0; i < reg.count(); ++i) {
                auto* plugin = reg.get(i);
                if (!plugin) continue;
                if (has_cap(plugin->capabilities(), PluginCapability::ProvidesTab)) {
                    auto* w = plugin->tab_widget();
                    int idx = tabs_->indexOf(w);
                    if (idx >= 0) {
                        tabs_->removeTab(idx);
                        w->setParent(nullptr);
                    }
                }
            }

            // Reload DLL plugins (compiled-in entries preserved)
            reg.reload_all(plugin_dir_);

            // Re-init new plugins and add their tabs
            PluginContext ctx;
            ctx.builder = builder_.get();
            ctx.meta_db = &meta_db_;
            ctx.parent_widget = this;
            reg.init_all(ctx);

            for (size_t i = 0; i < reg.count(); ++i) {
                auto* plugin = reg.get(i);
                if (!plugin) continue;
                if (has_cap(plugin->capabilities(), PluginCapability::ProvidesTab)) {
                    // Skip compiled-in tabs that are already present
                    int existing = tabs_->indexOf(plugin->tab_widget());
                    if (existing >= 0) continue;

                    auto* w = plugin->tab_widget();
                    auto title = plugin->tab_title();
                    if (w && !title.isEmpty()) {
                        int idx = tabs_->addTab(w, title);
                        tabs_->setTabEnabled(idx, false);
                        if (auto* obj = qobject_cast<QObject*>(w)) {
                            if (obj->metaObject()->indexOfSignal("contentChanged()") >= 0)
                                connect(obj, SIGNAL(contentChanged()), this, SLOT(on_content_changed()));
                        }
                    }
                }
            }

            // If a map is already loaded, reactivate all plugins
            if (!current_path_.isEmpty()) {
                reg.activate_all();
                for (int i = 0; i < tabs_->count(); ++i)
                    tabs_->setTabEnabled(i, true);
            }
        }

        save_settings();
    }
}

void MainWindow::save_settings() {
    QSettings qs;
    qs.setValue("BuildSettings/Mode",             static_cast<int>(settings_.mode));
    qs.setValue("BuildSettings/RemoveListfile",   settings_.remove_listfile);
    qs.setValue("BuildSettings/RemoveAttributes", settings_.remove_attributes);
    qs.setValue("Plugins/Directory",              plugin_dir_);
    qs.setValue("WC3Data/Directory",              wc3_data_dir_);
    qs.setValue("MainWindow/Geometry",            saveGeometry());
    qs.setValue("MainWindow/State",               saveState());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (modified_) {
        auto ret = QMessageBox::warning(this, tr("Unsaved Changes"),
            tr("Do you want to save changes before closing?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (ret == QMessageBox::Save) {
            on_save();
            event->accept();
        } else if (ret == QMessageBox::Discard) {
            event->accept();
        } else {
            event->ignore();
            return;
        }
    } else {
        event->accept();
    }

    if (event->isAccepted()) {
        PluginRegistry::instance().deactivate_all();
    }
}

void MainWindow::on_about() {
    QMessageBox::about(this, tr("About w3x-packer"),
        tr("w3x-packer v0.1\n"
           "Warcraft III .w3x map packing utility\n\n"
           "Built with StormLib + Qt"));
}
