#include "objecteditor.h"
#include "../src/core/map_builder.h"
#include "../src/core/types.h"
#include "../src/core/metadata.h"
#include "../src/core/object_id_gen.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFormLayout>
#include <QSplitter>
#include <QVBoxLayout>
#include <QCompleter>
#include <QPointer>
#include <QMessageBox>
#include <QApplication>

// ============================================================
// File type mapping (combo UI index → ObjectFileType / filename)
// ============================================================
static const char* kFileTypes[] = {
    "war3map.w3u", "war3map.w3a", "war3map.w3t", "war3map.w3h",
    "war3map.w3b", "war3map.w3d", "war3map.w3q"
};

static const char* kFileLabels[] = {
    QT_TRANSLATE_NOOP("ObjectEditor", "Units (w3u)"),
    QT_TRANSLATE_NOOP("ObjectEditor", "Abilities (w3a)"),
    QT_TRANSLATE_NOOP("ObjectEditor", "Items (w3t)"),
    QT_TRANSLATE_NOOP("ObjectEditor", "Buffs (w3h)"),
    QT_TRANSLATE_NOOP("ObjectEditor", "Doodads (w3b)"),
    QT_TRANSLATE_NOOP("ObjectEditor", "Destructables (w3d)"),
    QT_TRANSLATE_NOOP("ObjectEditor", "Upgrades (w3q)"),
};

// Combo index → ObjectFileType enum
int ObjectEditor::file_index_to_type(int combo_idx) {
    // kFileTypes order: w3u, w3a, w3t, w3h, w3b, w3d, w3q
    static const ObjectFileType map[] = {
        ObjectFileType::Units,
        ObjectFileType::Abilities,
        ObjectFileType::Items,
        ObjectFileType::Buffs,
        ObjectFileType::Destructables,
        ObjectFileType::Doodads,
        ObjectFileType::Upgrades,
    };
    if (combo_idx < 0 || combo_idx >= 7) return 0;
    return (int)map[combo_idx];
}

ObjectFileType ObjectEditor::combo_idx_to_file_type(int idx) {
    return static_cast<ObjectFileType>(file_index_to_type(idx));
}

const char* ObjectEditor::file_type_to_name(ObjectFileType type) {
    switch (type) {
        case ObjectFileType::Units:         return "Unit";
        case ObjectFileType::Items:         return "Item";
        case ObjectFileType::Abilities:     return "Ability";
        case ObjectFileType::Buffs:         return "Buff";
        case ObjectFileType::Upgrades:      return "Upgrade";
        case ObjectFileType::Destructables: return "Destructable";
        case ObjectFileType::Doodads:       return "Doodad";
    }
    return "Object";
}

// ============================================================
// Helpers
// ============================================================
static std::string mod_value_str(const Modification& m) {
    switch (m.type) {
        case ValueType::Int:    return std::to_string(m.int_val());
        case ValueType::Real:
        case ValueType::Unreal: return std::to_string(m.float_val());
        case ValueType::String: return m.str_val();
    }
    return "";
}

int ObjectEditor::current_file_type_index() const {
    return file_type_combo_ ? file_type_combo_->currentIndex() : 0;
}

// ============================================================
// Constructor
// ============================================================
ObjectEditor::ObjectEditor(QWidget* parent)
    : QWidget(parent)
{
    build_ui();
}

void ObjectEditor::build_ui() {
    auto* layout = new QVBoxLayout(this);

    // ── File type selector ──
    file_type_combo_ = new QComboBox;
    for (auto* label : kFileLabels)
        file_type_combo_->addItem(tr(label));
    connect(file_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ObjectEditor::on_file_type_changed);

    // ── Tree ──
    tree_ = new QTreeWidget;
    tree_->setHeaderLabels({tr("Object / Mod"), tr("Value")});
    tree_->header()->setStretchLastSection(true);
    tree_->setAlternatingRowColors(true);
    connect(tree_, &QTreeWidget::itemSelectionChanged,
        this, &ObjectEditor::on_item_selected);

    // ── Bottom buttons row ──
    add_mod_btn_ = new QPushButton(tr("Add Mod"));
    remove_mod_btn_ = new QPushButton(tr("Remove Mod"));
    create_obj_btn_ = new QPushButton(tr("Create Custom Object"));
    add_mod_btn_->setEnabled(false);
    remove_mod_btn_->setEnabled(false);

    connect(add_mod_btn_, &QPushButton::clicked, this, &ObjectEditor::on_add_mod);
    connect(remove_mod_btn_, &QPushButton::clicked, this, &ObjectEditor::on_remove_mod);
    connect(create_obj_btn_, &QPushButton::clicked, this, &ObjectEditor::on_create_custom_object);

    auto* btn_row = new QHBoxLayout;
    btn_row->addWidget(create_obj_btn_);
    btn_row->addStretch();
    btn_row->addWidget(add_mod_btn_);
    btn_row->addWidget(remove_mod_btn_);

    // Top widget: tree + buttons
    auto* top_widget = new QWidget;
    auto* top_layout = new QVBoxLayout(top_widget);
    top_layout->setContentsMargins(0, 0, 0, 0);
    top_layout->addWidget(tree_, 1);
    top_layout->addLayout(btn_row);

    // ── Mod detail panel ──
    detail_panel_ = new QGroupBox(tr("Modification Details"));
    auto* detail_form = new QFormLayout(detail_panel_);

    // mod_id combo box (replaces old QLineEdit)
    mod_id_combo_ = new QComboBox;
    mod_id_combo_->setEditable(true);
    mod_id_combo_->setInsertPolicy(QComboBox::NoInsert);
    connect(mod_id_combo_, &QComboBox::editTextChanged,
        this, &ObjectEditor::on_mod_id_changed);

    value_type_combo_ = new QComboBox;
    value_type_combo_->addItems({tr("Int"), tr("Real"), tr("Unreal"), tr("String")});

    level_spin_ = new QSpinBox;
    level_spin_->setRange(0, 999);
    data_ptr_spin_ = new QSpinBox;
    data_ptr_spin_->setRange(0, 999999);

    // Value input: stacked widget
    value_stack_ = new QStackedWidget;

    value_int_ = new QSpinBox;
    value_int_->setRange(-999999, 9999999);
    value_stack_->addWidget(value_int_); // page 0 → Int

    value_float_ = new QDoubleSpinBox;
    value_float_->setRange(-999999.0, 9999999.0);
    value_float_->setDecimals(4);
    value_stack_->addWidget(value_float_); // page 1 → Real/Unreal

    value_str_ = new QLineEdit;
    value_stack_->addWidget(value_str_); // page 2 → String

    detail_form->addRow(tr("Mod ID:"), mod_id_combo_);
    detail_form->addRow(tr("Type:"), value_type_combo_);
    detail_form->addRow(tr("Level:"), level_spin_);
    detail_form->addRow(tr("Data Ptr:"), data_ptr_spin_);
    detail_form->addRow(tr("Value:"), value_stack_);

    apply_mod_btn_ = new QPushButton(tr("Apply"));
    connect(apply_mod_btn_, &QPushButton::clicked, this, &ObjectEditor::on_apply_mod);

    auto* detail_btn_row = new QHBoxLayout;
    detail_btn_row->addStretch();
    detail_btn_row->addWidget(apply_mod_btn_);
    detail_form->addRow(detail_btn_row);
    detail_panel_->hide();

    // ── Splitter ──
    splitter_ = new QSplitter(Qt::Vertical);
    splitter_->addWidget(top_widget);
    splitter_->addWidget(detail_panel_);

    layout->addWidget(file_type_combo_);
    layout->addWidget(splitter_, 1);
}

// ============================================================
// Load / Clear
// ============================================================
void ObjectEditor::load(MapBuilder* builder) {
    builder_ = builder;
    clear();
    if (!builder_) return;

    for (auto* name : kFileTypes) {
        try {
            files_[name] = builder_->read_object(name);
        } catch (...) {
            files_[name] = ObjectFile{};
        }
    }

    current_file_ = kFileTypes[0];
    file_type_combo_->setCurrentIndex(0);
    populate_objects(files_[current_file_]);
    refresh_known_objects();
}

void ObjectEditor::clear() {
    tree_->clear();
    files_.clear();
    current_file_.clear();
    current_section_.clear();
    current_entry_index_ = -1;
    current_mod_index_ = -1;
    detail_panel_->hide();
    add_mod_btn_->setEnabled(false);
    remove_mod_btn_->setEnabled(false);
    apply_mod_btn_->setEnabled(false);
}

void ObjectEditor::sync_to_builder() {
    if (!builder_) return;
    for (auto& [name, file] : files_) {
        builder_->set_object(name, file);
    }
}

// ============================================================
// File type switch
// ============================================================
void ObjectEditor::on_file_type_changed(int index) {
    if (index < 0 || index >= 7) return;
    current_file_ = kFileTypes[index];

    auto it = files_.find(current_file_);
    if (it != files_.end())
        populate_objects(it->second);
}

// ============================================================
// Refresh known objects into file type combo tooltips / data
// ============================================================
void ObjectEditor::refresh_known_objects() {
    // When metadata is available, populate the "Create Custom Object"
    // dropdown data — no-op here, handled inside on_create_custom_object.
}

// ============================================================
// Populate tree
// ============================================================
void ObjectEditor::populate_objects(const ObjectFile& obj) {
    tree_->clear();
    detail_panel_->hide();
    add_mod_btn_->setEnabled(false);
    remove_mod_btn_->setEnabled(false);
    apply_mod_btn_->setEnabled(false);
    current_entry_index_ = -1;
    current_mod_index_ = -1;

    // Cache the current file type for metadata lookups
    int ft_idx = current_file_type_index();
    auto obj_type = combo_idx_to_file_type(ft_idx);

    auto add_entries = [this, &obj_type](
        const std::vector<ObjectEntry>& entries, const char* section)
    {
        for (size_t ei = 0; ei < entries.size(); ++ei) {
            auto& e = entries[ei];
            QString orig_id = QString::fromStdString(e.original_id.str());

            // Resolve display name from *strings.txt
            QString orig_name;
            if (meta_db_) {
                auto* name = meta_db_->find_object_name(e.original_id.str());
                if (name)
                    orig_name = QString::fromStdString(*name);
            }

            QString label;
            if (orig_name.isEmpty()) {
                label = QString("(%1) → (%2)")
                    .arg(orig_id)
                    .arg(e.new_id.is_null()
                        ? tr("mod")
                        : QString::fromStdString(e.new_id.str()));
            } else {
                label = QString("%1 (%2) → (%3)")
                    .arg(orig_name)
                    .arg(orig_id)
                    .arg(e.new_id.is_null()
                        ? tr("mod")
                        : QString::fromStdString(e.new_id.str()));
            }

            auto* item = new QTreeWidgetItem;
            item->setText(0, label);
            item->setData(0, Qt::UserRole,
                QString("entry:%1:%2").arg(section).arg(ei));

            for (size_t mi = 0; mi < e.mods.size(); ++mi) {
                auto& m = e.mods[mi];
                auto* child = new QTreeWidgetItem;

                // Show field display name if available
                QString mod_label;
                if (meta_db_) {
                    auto* mf = meta_db_->find_field(obj_type, m.mod_id.str());
                    if (mf && !mf->display_name.empty()) {
                        mod_label = QString("%1 - %2")
                            .arg(QString::fromStdString(m.mod_id.str()))
                            .arg(QString::fromStdString(mf->display_name));
                    }
                }
                if (mod_label.isEmpty())
                    mod_label = QString::fromStdString(m.mod_id.str());

                child->setText(0, mod_label);
                child->setText(1, QString::fromStdString(mod_value_str(m)));
                child->setData(0, Qt::UserRole,
                    QString("mod:%1:%2:%3").arg(section).arg(ei).arg(mi));
                item->addChild(child);
            }
            tree_->addTopLevelItem(item);
        }
    };

    add_entries(obj.original_objects, "orig");
    add_entries(obj.custom_objects, "custom");
    tree_->expandAll();
}

// ============================================================
// Item selection
// ============================================================
void ObjectEditor::on_item_selected() {
    auto items = tree_->selectedItems();
    if (items.isEmpty()) {
        detail_panel_->hide();
        add_mod_btn_->setEnabled(false);
        remove_mod_btn_->setEnabled(false);
        apply_mod_btn_->setEnabled(false);
        return;
    }

    auto tag = items.first()->data(0, Qt::UserRole).toString();
    auto parts = tag.split(':');
    if (parts.size() < 3) return;

    QString type = parts[0];
    QString section = parts[1];
    int entry_idx = parts[2].toInt();
    auto& file = files_[current_file_];
    auto& entries = (section == "orig")
        ? file.original_objects : file.custom_objects;

    if (entry_idx >= (int)entries.size()) return;

    if (type == "entry") {
        current_section_ = section.toStdString();
        current_entry_index_ = entry_idx;
        current_mod_index_ = -1;
        add_mod_btn_->setEnabled(true);
        remove_mod_btn_->setEnabled(false);
        detail_panel_->hide();
        apply_mod_btn_->setEnabled(false);
    } else if (type == "mod" && parts.size() >= 4) {
        int mod_idx = parts[3].toInt();
        current_section_ = section.toStdString();
        current_entry_index_ = entry_idx;
        current_mod_index_ = mod_idx;

        if (mod_idx < (int)entries[entry_idx].mods.size()) {
            show_mod_details(&entries[entry_idx].mods[mod_idx]);
            add_mod_btn_->setEnabled(true);
            remove_mod_btn_->setEnabled(true);
            detail_panel_->show();
            apply_mod_btn_->setEnabled(true);
        }
    }
}

// ============================================================
// Show mod details (with metadata awareness)
// ============================================================
void ObjectEditor::show_mod_details(const Modification* mod) {
    // Refresh the mod_id combo with known fields for this file type
    refresh_mod_id_combo();

    QString mod_id = QString::fromStdString(mod->mod_id.str());
    mod_id_combo_->setCurrentText(mod_id);

    value_type_combo_->setCurrentIndex((int)mod->type);
    level_spin_->setValue(mod->level);
    data_ptr_spin_->setValue(mod->data_ptr);

    // Look up field in metadata to set the value widget type
    ValueType vtype = mod->type;
    double min_val = 0;
    double max_val = 9999999;
    if (meta_db_) {
        int ft_idx = current_file_type_index();
        auto obj_type = combo_idx_to_file_type(ft_idx);
        auto* mf = meta_db_->find_field(obj_type, mod->mod_id.str());
        if (mf) {
            vtype = mf->type;
            min_val = mf->min_val;
            max_val = mf->max_val;
        }
    }

    update_value_widget(vtype, min_val, max_val);

    // Set the value
    switch (vtype) {
        case ValueType::Int:
            value_int_->setValue(mod->int_val());
            break;
        case ValueType::Real:
        case ValueType::Unreal:
            value_float_->setValue(mod->float_val());
            break;
        case ValueType::String:
            value_str_->setText(QString::fromStdString(mod->str_val()));
            break;
        default:
            value_str_->setText(QString::fromStdString(mod->str_val()));
            break;
    }
}

// ============================================================
// Refresh the mod_id combo with metadata fields (if available)
// ============================================================
void ObjectEditor::refresh_mod_id_combo() {
    mod_id_combo_->blockSignals(true);
    mod_id_combo_->clear();

    if (meta_db_) {
        int ft_idx = current_file_type_index();
        auto obj_type = combo_idx_to_file_type(ft_idx);
        const auto& fields = meta_db_->get_fields(obj_type);

        if (!fields.empty()) {
            for (const auto& mf : fields) {
                QString label = QString("%1 - %2")
                    .arg(QString::fromStdString(mf.id))
                    .arg(QString::fromStdString(mf.display_name));
                mod_id_combo_->addItem(label, QString::fromStdString(mf.id));
            }

            // Set up autocomplete
            auto* completer = new QCompleter(mod_id_combo_->model(), mod_id_combo_);
            completer->setCaseSensitivity(Qt::CaseInsensitive);
            completer->setCompletionMode(QCompleter::PopupCompletion);
            completer->setFilterMode(Qt::MatchContains);
            mod_id_combo_->setCompleter(completer);
        } else {
            // Fallback: no metadata, free-form
            mod_id_combo_->setEditable(true);
        }
    }

    mod_id_combo_->blockSignals(false);
}

// ============================================================
// Switch the value widget based on field type
// ============================================================
void ObjectEditor::update_value_widget(ValueType type, double min_val, double max_val) {
    switch (type) {
        case ValueType::Int: {
            value_int_->setRange((int)min_val, (int)max_val);
            value_stack_->setCurrentIndex(0);
            break;
        }
        case ValueType::Real:
        case ValueType::Unreal: {
            value_float_->setRange(min_val, max_val);
            value_stack_->setCurrentIndex(1);
            break;
        }
        case ValueType::String:
        case ValueType::Bool:
        case ValueType::Char:
        default: {
            value_stack_->setCurrentIndex(2);
            break;
        }
    }
}

// ============================================================
// When user types in mod_id combo, update value widget if field
// type differs from the current value_type_combo_ selection
// ============================================================
void ObjectEditor::on_mod_id_changed(const QString& text) {
    if (!meta_db_)
        return;

    int ft_idx = current_file_type_index();
    auto obj_type = combo_idx_to_file_type(ft_idx);
    auto* mf = meta_db_->find_field(obj_type, text.toStdString());
    if (mf) {
        value_type_combo_->setCurrentIndex((int)mf->type);
        update_value_widget(mf->type, mf->min_val, mf->max_val);
    }
}

// ============================================================
// Apply modification
// ============================================================
void ObjectEditor::on_apply_mod() {
    if (current_mod_index_ < 0) return;

    auto& file = files_[current_file_];
    auto& entries = (current_section_ == "orig")
        ? file.original_objects : file.custom_objects;
    if (current_entry_index_ >= (int)entries.size()) return;
    if (current_mod_index_ >= (int)entries[current_entry_index_].mods.size())
        return;

    auto& mod = entries[current_entry_index_].mods[current_mod_index_];

    // Extract mod ID from combo (user data if available, else text)
    QString mod_id_text = mod_id_combo_->currentText();
    auto combo_data = mod_id_combo_->currentData();
    if (combo_data.isValid())
        mod_id_text = combo_data.toString();
    mod.mod_id = ObjectID::from_string(mod_id_text.toStdString());

    mod.type = (ValueType)value_type_combo_->currentIndex();
    mod.level = level_spin_->value();
    mod.data_ptr = data_ptr_spin_->value();

    // Read value from the active widget
    switch (mod.type) {
        case ValueType::Int:
            mod.value = value_int_->value();
            break;
        case ValueType::Real:
        case ValueType::Unreal:
            mod.value = (float)value_float_->value();
            break;
        case ValueType::String:
        default:
            mod.value = value_str_->text().toStdString();
            break;
    }

    populate_objects(files_[current_file_]);
    emit contentChanged();
}

// ============================================================
// Add / Remove mod
// ============================================================
void ObjectEditor::on_add_mod() {
    auto& file = files_[current_file_];
    auto& entries = (current_section_ == "orig")
        ? file.original_objects : file.custom_objects;
    if (current_entry_index_ >= (int)entries.size()) return;

    Modification m;
    m.mod_id = ObjectID::from_string("XXXX");
    m.type = ValueType::Int;
    m.level = 0;
    m.data_ptr = 0;
    m.value = int32_t(0);
    entries[current_entry_index_].mods.push_back(m);

    populate_objects(files_[current_file_]);
    emit contentChanged();
}

void ObjectEditor::on_remove_mod() {
    if (current_mod_index_ < 0) return;

    auto& file = files_[current_file_];
    auto& entries = (current_section_ == "orig")
        ? file.original_objects : file.custom_objects;
    if (current_entry_index_ >= (int)entries.size()) return;
    if (current_mod_index_ >= (int)entries[current_entry_index_].mods.size())
        return;

    entries[current_entry_index_].mods.erase(
        entries[current_entry_index_].mods.begin() + current_mod_index_);
    current_mod_index_ = -1;

    populate_objects(files_[current_file_]);
    emit contentChanged();
}

// ============================================================
// Create Custom Object
// ============================================================
void ObjectEditor::on_create_custom_object() {
    int ft_idx = current_file_type_index();
    auto obj_type = combo_idx_to_file_type(ft_idx);
    const char* type_name = file_type_to_name(obj_type);

    auto& file = files_[current_file_];

    // Build a simple dialog to pick the base object
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Create Custom %1").arg(type_name));
    dlg.setMinimumWidth(400);

    auto* dlg_layout = new QVBoxLayout(&dlg);

    // Base object selector
    auto* base_layout = new QFormLayout;

    auto* base_combo = new QComboBox;
    base_combo->setEditable(true);
    base_combo->setInsertPolicy(QComboBox::NoInsert);

    // Populate with known objects from metadata, plus existing original objects
    if (meta_db_) {
        const auto& known = meta_db_->get_known_objects(obj_type);
        for (const auto& [id, name] : known) {
            QString label = QString("%1 - %2")
                .arg(QString::fromStdString(id))
                .arg(QString::fromStdString(name));
            base_combo->addItem(label, QString::fromStdString(id));
        }
        if (!known.empty())
            base_combo->setCurrentIndex(0);
    }

    // Add original objects from the current file
    for (const auto& entry : file.original_objects) {
        QString id_str = QString::fromStdString(entry.original_id.str());
        // Check if already in combo
        bool found = false;
        for (int i = 0; i < base_combo->count(); ++i) {
            if (base_combo->itemData(i).toString() == id_str) {
                found = true;
                break;
            }
        }
        if (!found) {
            base_combo->addItem(id_str, id_str);
        }
    }

    base_layout->addRow(tr("Base Object:"), base_combo);

    // New ID display (auto-generated)
    auto* new_id_label = new QLabel;
    new_id_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    base_layout->addRow(tr("New ID:"), new_id_label);

    // Update ID when base changes
    auto update_new_id = [&]() {
        QString base_text = base_combo->currentData().toString();
        if (base_text.isEmpty())
            base_text = base_combo->currentText();
        if (base_text.length() >= 1) {
            ObjectID base_oid = ObjectID::from_string(base_text.toStdString());
            ObjectIDGenerator gen;
            gen.seed_with(file);
            ObjectID new_oid = gen.generate(base_oid);
            new_id_label->setText(QString::fromStdString(new_oid.str()));
        }
    };
    QObject::connect(base_combo, &QComboBox::currentTextChanged,
        &dlg, [update_new_id](const QString&) { update_new_id(); });
    QObject::connect(base_combo, &QComboBox::editTextChanged,
        &dlg, [update_new_id](const QString&) { update_new_id(); });

    dlg_layout->addLayout(base_layout);

    // Buttons
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    dlg_layout->addWidget(buttons);

    // Initial ID generation
    update_new_id();

    if (dlg.exec() != QDialog::Accepted)
        return;

    // Create the new custom object entry
    QString base_text = base_combo->currentData().toString();
    if (base_text.isEmpty())
        base_text = base_combo->currentText();
    if (base_text.length() < 1) {
        QMessageBox::warning(this, tr("Error"),
            tr("Please select or enter a valid base object ID."));
        return;
    }

    ObjectID base_oid = ObjectID::from_string(base_text.toStdString());
    ObjectIDGenerator gen;
    gen.seed_with(file);
    ObjectID new_oid = gen.generate(base_oid);

    // Create the entry
    ObjectEntry entry;
    entry.original_id = base_oid;
    entry.new_id = new_oid;
    // Copy modifications from the original object (if any)
    auto orig_it = std::find_if(file.original_objects.begin(), file.original_objects.end(),
        [&](const ObjectEntry& e) { return e.original_id == base_oid; });
    if (orig_it != file.original_objects.end()) {
        entry.mods = orig_it->mods;
    }

    file.custom_objects.push_back(std::move(entry));
    populate_objects(file);
    emit contentChanged();
}

// ============================================================
// ObjectEditorPlugin
// ============================================================
bool ObjectEditorPlugin::init(PluginContext& ctx) {
    builder_ = ctx.builder;
    editor_ = new ObjectEditor(ctx.parent_widget);
    editor_->set_meta_db(ctx.meta_db);
    connect(editor_, &ObjectEditor::contentChanged, this, &ObjectEditorPlugin::contentChanged);
    return true;
}

void ObjectEditorPlugin::activate() {
    if (editor_ && builder_)
        editor_->load(builder_);
}

void ObjectEditorPlugin::deactivate() {
    if (editor_) editor_->clear();
}

void ObjectEditorPlugin::sync_to_builder() {
    if (editor_) editor_->sync_to_builder();
}

void ObjectEditorPlugin::destroy() {
    delete editor_;
    editor_ = nullptr;
    builder_ = nullptr;
}
