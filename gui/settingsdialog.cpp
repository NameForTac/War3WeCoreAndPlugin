#include "settingsdialog.h"
#include "../src/core/map_builder.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Build Settings"));
    setMinimumWidth(420);
    build_ui();
}

void SettingsDialog::build_ui() {
    auto* layout = new QVBoxLayout(this);

    // ── Output Options ──
    auto* g = new QGroupBox(tr("Output Options"));
    auto* f = new QFormLayout(g);

    mode_combo_ = new QComboBox;
    mode_combo_->addItems({tr("Binary (OBJ)"), tr("Spreadsheet (SLK)"), tr("Text (LNI)")});

    remove_listfile_ = new QCheckBox(tr("Remove (listfile) from output"));
    remove_listfile_->setChecked(true);

    remove_attributes_ = new QCheckBox(tr("Remove (attributes) from output"));
    remove_attributes_->setChecked(true);

    f->addRow(tr("Output Mode:"), mode_combo_);
    f->addRow(tr("Listfile:"), remove_listfile_);
    f->addRow(tr("Attributes:"), remove_attributes_);

    layout->addWidget(g);

    // ── Plugin Directory ──
    auto* pg = new QGroupBox(tr("Plugins"));
    auto* pf = new QFormLayout(pg);

    auto* plugin_row = new QHBoxLayout;
    plugin_dir_edit_ = new QLineEdit;
    plugin_dir_edit_->setPlaceholderText(tr("Path to plugins directory"));
    auto* plugin_browse = new QPushButton(tr("Browse..."));
    connect(plugin_browse, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Plugins Directory"),
            plugin_dir_edit_->text());
        if (!dir.isEmpty())
            plugin_dir_edit_->setText(dir);
    });
    plugin_row->addWidget(plugin_dir_edit_);
    plugin_row->addWidget(plugin_browse);

    pf->addRow(tr("Plugin Dir:"), plugin_row);
    layout->addWidget(pg);

    // ── WC3 Data Directory ──
    auto* wg = new QGroupBox(tr("Warcraft III Data"));
    auto* wf = new QFormLayout(wg);

    auto* data_row = new QHBoxLayout;
    wc3_data_dir_edit_ = new QLineEdit;
    wc3_data_dir_edit_->setPlaceholderText(tr("Path to Warcraft III installation (e.g. C:/Program Files/Warcraft III)"));
    auto* data_browse = new QPushButton(tr("Browse..."));
    connect(data_browse, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Warcraft III Directory"),
            wc3_data_dir_edit_->text());
        if (!dir.isEmpty())
            wc3_data_dir_edit_->setText(dir);
    });
    data_row->addWidget(wc3_data_dir_edit_);
    data_row->addWidget(data_browse);

    wf->addRow(tr("WC3 Data Dir:"), data_row);

    auto* info_label = new QLabel(tr("Used to load UnitMetaData.slk etc. for field autocomplete in the Object Editor."));
    info_label->setWordWrap(true);
    info_label->setStyleSheet("color: gray; font-size: 11px;");
    wf->addRow(info_label);

    layout->addWidget(wg);

    // ── Buttons ──
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

BuildSettings SettingsDialog::settings() const {
    BuildSettings s;
    s.mode = static_cast<OutputMode>(mode_combo_->currentIndex());
    s.remove_listfile = remove_listfile_->isChecked();
    s.remove_attributes = remove_attributes_->isChecked();
    return s;
}

void SettingsDialog::set_settings(const BuildSettings& s) {
    mode_combo_->setCurrentIndex(static_cast<int>(s.mode));
    remove_listfile_->setChecked(s.remove_listfile);
    remove_attributes_->setChecked(s.remove_attributes);
}

QString SettingsDialog::plugin_directory() const {
    return plugin_dir_edit_->text();
}

void SettingsDialog::set_plugin_directory(const QString& path) {
    plugin_dir_edit_->setText(path);
}

QString SettingsDialog::wc3_data_directory() const {
    return wc3_data_dir_edit_->text();
}

void SettingsDialog::set_wc3_data_directory(const QString& path) {
    wc3_data_dir_edit_->setText(path);
}
