#pragma once

#include <QMainWindow>
#include "terrain_edit_types.h"
#include "../../src/core/w3e.h"

class TerrainEditWidget;
class MapBuilder;
class QSlider;
class QLabel;
class QToolButton;
class QComboBox;
class QShortcut;
enum class RenderMode;

class TerrainEditWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit TerrainEditWindow(QWidget* parent = nullptr);
    ~TerrainEditWindow() override;

    void onMapLoaded(MapBuilder* builder);
    void onMapClosed();
    void syncToBuilder(MapBuilder* builder);
    bool isModified() const { return modified_; }
    void setWc3DataDir(const QString& dir) { wc3_data_dir_ = dir; }

signals:
    void contentChanged();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onTerrainEdited();
    void onToolChanged(EditTool tool);
    void onBrushSizeChanged(int value);
    void onBrushStrengthChanged(int value);
    void onTextureChanged(int index);
    void onShowTextureToggled(bool checked);

private:
    void createToolbar();
    void createShortcuts();
    void saveSettings();
    void restoreSettings();
    void updateTextureCombo();
    void selectTool(EditTool tool);

    static QIcon makeIcon(const QString& text, const QColor& fg);

    TerrainEditWidget* widget_ = nullptr;
    EditTool current_tool_ = EditTool::Raise;
    int brush_size_ = 4;
    float brush_strength_ = 1.0f;
    int paint_texture_ = 0;
    BrushShape brush_shape_ = BrushShape::Circle;
    bool show_texture_ = true;
    bool modified_ = false;

    QToolButton* raise_btn_ = nullptr;
    QToolButton* lower_btn_ = nullptr;
    QToolButton* smooth_btn_ = nullptr;
    QToolButton* flatten_btn_ = nullptr;
    QToolButton* paint_btn_ = nullptr;
    QToolButton* texture_toggle_btn_ = nullptr;
    QToolButton* brush_shape_btn_ = nullptr;
    QSlider* brush_size_slider_ = nullptr;
    QSlider* brush_strength_slider_ = nullptr;
    QLabel* brush_size_label_ = nullptr;
    QLabel* brush_strength_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QComboBox* texture_combo_ = nullptr;
    QComboBox* render_mode_combo_ = nullptr;

    Terrain terrain_;
    bool has_terrain_ = false;
    QString wc3_data_dir_;
};
