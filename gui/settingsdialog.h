#pragma once

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>

struct BuildSettings;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    BuildSettings settings() const;
    void set_settings(const BuildSettings& s);

    QString plugin_directory() const;
    void set_plugin_directory(const QString& path);

    QString wc3_data_directory() const;
    void set_wc3_data_directory(const QString& path);

private:
    void build_ui();

    QComboBox* mode_combo_ = nullptr;
    QCheckBox* remove_listfile_ = nullptr;
    QCheckBox* remove_attributes_ = nullptr;
    QLineEdit* plugin_dir_edit_ = nullptr;
    QLineEdit* wc3_data_dir_edit_ = nullptr;
};
