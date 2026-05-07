#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QStackedWidget>
#include <QLabel>
#include "../src/core/types.h"
#include "../src/core/plugin.h"
#include <map>
#include <string>

class MapBuilder;
class MetaDataDB;
class QSplitter;
struct ObjectFile;
struct Modification;

class ObjectEditor : public QWidget {
    Q_OBJECT
public:
    explicit ObjectEditor(QWidget* parent = nullptr);

    void load(MapBuilder* builder);
    void sync_to_builder();
    void clear();

    void set_meta_db(MetaDataDB* db) { meta_db_ = db; }

signals:
    void contentChanged();

private slots:
    void on_file_type_changed(int index);
    void on_item_selected();
    void on_add_mod();
    void on_remove_mod();
    void on_apply_mod();
    void on_mod_id_changed(const QString& text);
    void on_create_custom_object();

private:
    void build_ui();
    void populate_objects(const ObjectFile& obj);
    void show_mod_details(const Modification* mod);
    void refresh_mod_id_combo();
    void refresh_known_objects();
    void update_value_widget(ValueType type, double min_val, double max_val);
    int current_file_type_index() const;

    // --- Helpers ---
    static int file_index_to_type(int combo_idx);
    static ObjectFileType combo_idx_to_file_type(int idx);
    static const char* file_type_to_name(ObjectFileType type);

    QComboBox* file_type_combo_ = nullptr;
    QTreeWidget* tree_ = nullptr;

    // Mod detail editing
    QWidget* detail_panel_ = nullptr;
    QComboBox* mod_id_combo_ = nullptr;
    QComboBox* value_type_combo_ = nullptr;
    QSpinBox* level_spin_ = nullptr;
    QSpinBox* data_ptr_spin_ = nullptr;

    // Value input: stacked for type-aware widgets
    QStackedWidget* value_stack_ = nullptr;
    QSpinBox* value_int_ = nullptr;
    QDoubleSpinBox* value_float_ = nullptr;
    QLineEdit* value_str_ = nullptr;

    QPushButton* apply_mod_btn_ = nullptr;
    QPushButton* add_mod_btn_ = nullptr;
    QPushButton* remove_mod_btn_ = nullptr;
    QPushButton* create_obj_btn_ = nullptr;

    MapBuilder* builder_ = nullptr;
    MetaDataDB* meta_db_ = nullptr;
    QSplitter* splitter_ = nullptr;
    std::map<std::string, ObjectFile> files_;
    std::string current_file_;
    std::string current_section_;
    int current_entry_index_ = -1;
    int current_mod_index_ = -1;
};

// ============================================================
// ObjectEditorPlugin — IEditorPlugin wrapper for ObjectEditor
// ============================================================
class ObjectEditorPlugin : public QObject, public IEditorPlugin {
    Q_OBJECT
public:
    ObjectEditorPlugin(MetaDataDB* db) : meta_db_(db) {}

    QString name() const override { return tr("Object Editor"); }
    QString version() const override { return "1.0"; }
    PluginCapability capabilities() const override {
        return PluginCapability::ProvidesTab | PluginCapability::NeedsSavable;
    }

    bool init(PluginContext& ctx) override;
    void activate() override;
    void deactivate() override;
    void destroy() override;

    QWidget* tab_widget() override { return editor_; }
    QString tab_title() override { return tr("Object Editor"); }
    void sync_to_builder() override;

signals:
    void contentChanged();

private:
    ObjectEditor* editor_ = nullptr;
    MapBuilder* builder_ = nullptr;
    MetaDataDB* meta_db_ = nullptr;
};
