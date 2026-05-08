#include "terrain_edit_window.h"
#include "terrain_edit_widget.h"
#include "../../src/core/map_builder.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QToolButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QStatusBar>
#include <QButtonGroup>
#include <QShortcut>
#include <QCloseEvent>
#include <QSettings>
#include <QApplication>
#include <algorithm>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStyle>

// ============================================================
// Icon factory
// ============================================================
static QIcon make_icon_raise() {
    QPixmap pm(28, 28); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(100, 200, 100), 2); p.setPen(pen);
    p.drawLine(14, 22, 14, 6);
    p.drawLine(8, 12, 14, 6);
    p.drawLine(20, 12, 14, 6);
    // terrain base
    pen.setColor(QColor(100, 200, 100, 80)); p.setPen(pen);
    p.drawLine(2, 24, 8, 20); p.drawLine(8, 20, 14, 14);
    p.drawLine(14, 14, 20, 20); p.drawLine(20, 20, 26, 24);
    p.end(); return QIcon(pm);
}
static QIcon make_icon_lower() {
    QPixmap pm(28, 28); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(200, 140, 80), 2); p.setPen(pen);
    p.drawLine(14, 6, 14, 22);
    p.drawLine(8, 18, 14, 22);
    p.drawLine(20, 18, 14, 22);
    // terrain base
    pen.setColor(QColor(200, 140, 80, 80)); p.setPen(pen);
    p.drawLine(2, 10, 8, 14); p.drawLine(8, 14, 14, 8);
    p.drawLine(14, 8, 20, 14); p.drawLine(20, 14, 26, 10);
    p.end(); return QIcon(pm);
}
static QIcon make_icon_smooth() {
    QPixmap pm(28, 28); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(140, 180, 220), 2); p.setPen(pen);
    // smooth sine-like wave
    QPainterPath path; path.moveTo(2, 20);
    path.cubicTo(8, 8, 14, 8, 14, 14);
    path.cubicTo(14, 20, 20, 20, 26, 10);
    p.drawPath(path);
    p.end(); return QIcon(pm);
}
static QIcon make_icon_flatten() {
    QPixmap pm(28, 28); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(200, 200, 100), 2); p.setPen(pen);
    p.drawLine(2, 16, 26, 16);
    // arrows on top
    p.drawLine(14, 8, 14, 14); p.drawLine(10, 12, 14, 14); p.drawLine(18, 12, 14, 14);
    p.drawLine(14, 24, 14, 18); p.drawLine(10, 20, 14, 18); p.drawLine(18, 20, 14, 18);
    p.end(); return QIcon(pm);
}
static QIcon make_icon_paint() {
    QPixmap pm(28, 28); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(220, 140, 200), 2); p.setPen(pen);
    p.setBrush(QColor(220, 140, 200, 100));
    p.drawEllipse(6, 6, 16, 16);
    // paint drop
    p.drawEllipse(14, 16, 6, 6);
    p.end(); return QIcon(pm);
}
static QIcon make_icon_circle() {
    QPixmap pm(28, 28); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(200, 200, 80), 2); p.setPen(pen);
    p.drawEllipse(4, 4, 20, 20);
    p.end(); return QIcon(pm);
}
static QIcon make_icon_square() {
    QPixmap pm(28, 28); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(200, 200, 80), 2); p.setPen(pen);
    p.drawRect(4, 4, 20, 20);
    p.end(); return QIcon(pm);
}
static QIcon make_icon_texture() {
    QPixmap pm(28, 28); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(160, 220, 160), 2); p.setPen(pen);
    p.setBrush(QColor(120, 180, 120, 80));
    // checkerboard hint
    p.drawRect(4, 4, 9, 9); p.drawRect(15, 15, 9, 9);
    pen.setColor(QColor(80, 140, 80)); p.setPen(pen);
    p.drawRect(15, 4, 9, 9); p.drawRect(4, 15, 9, 9);
    p.end(); return QIcon(pm);
}

// ============================================================
// Construction
// ============================================================
TerrainEditWindow::TerrainEditWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Terrain Editor"));
    resize(900, 650);
    setObjectName("TerrainEditWindow");

    widget_ = new TerrainEditWidget(this);
    setCentralWidget(widget_);

    createToolbar();
    createShortcuts();
    restoreSettings();

    statusBar()->showMessage(tr("No map loaded"));

    connect(widget_, &TerrainEditWidget::terrainEdited,
            this, &TerrainEditWindow::onTerrainEdited);
    connect(widget_, &TerrainEditWidget::statusMessage,
            this, [this](const QString& msg) {
        if (msg.isEmpty()) {
            // Restore current tool name
            static const QString names[] = {
                tr("Raise"), tr("Lower"), tr("Smooth"),
                tr("Flatten"), tr("Paint")
            };
            int idx = static_cast<int>(current_tool_);
            status_label_->setText((idx >= 0 && idx < 5) ? names[idx] : QString());
        } else {
            status_label_->setText(msg);
        }
    });
}

TerrainEditWindow::~TerrainEditWindow() {
    saveSettings();
}

// ============================================================
// Toolbar
// ============================================================
void TerrainEditWindow::createToolbar() {
    auto* tb = new QToolBar(tr("Terrain Tools"), this);
    tb->setIconSize(QSize(28, 28));
    tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    tb->setMovable(false);
    addToolBar(Qt::LeftToolBarArea, tb);

    auto* group = new QButtonGroup(this);
    group->setExclusive(true);

    auto make_tool_btn = [&](const QIcon& icon, const QString& text,
                             const QString& tip, EditTool tool) -> QToolButton*
    {
        auto* btn = new QToolButton(tb);
        btn->setIcon(icon);
        btn->setText(text);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setCheckable(true);
        btn->setToolTip(tip);
        btn->setMinimumWidth(44);
        group->addButton(btn, static_cast<int>(tool));
        return btn;
    };

    raise_btn_   = make_tool_btn(make_icon_raise(),  tr("Raise"),
                                  tr("Raise terrain (1)"),  EditTool::Raise);
    lower_btn_   = make_tool_btn(make_icon_lower(),  tr("Lower"),
                                  tr("Lower terrain (2)"),  EditTool::Lower);
    smooth_btn_  = make_tool_btn(make_icon_smooth(), tr("Smooth"),
                                  tr("Smooth terrain (3)"), EditTool::Smooth);
    flatten_btn_ = make_tool_btn(make_icon_flatten(), tr("Flatten"),
                                  tr("Flatten terrain (4)"), EditTool::Flatten);
    paint_btn_   = make_tool_btn(make_icon_paint(),  tr("Paint"),
                                  tr("Paint texture (5)"),  EditTool::Paint);

    raise_btn_->setChecked(true);

    tb->addWidget(raise_btn_);
    tb->addWidget(lower_btn_);
    tb->addWidget(smooth_btn_);
    tb->addWidget(flatten_btn_);
    tb->addWidget(paint_btn_);

    connect(group, &QButtonGroup::idClicked, this, [this](int id) {
        onToolChanged(static_cast<EditTool>(id));
    });

    tb->addSeparator();

    // Brush size
    auto* size_lbl = new QLabel(tr("Size"), tb);
    size_lbl->setAlignment(Qt::AlignCenter);
    tb->addWidget(size_lbl);
    brush_size_slider_ = new QSlider(Qt::Horizontal, tb);
    brush_size_slider_->setRange(1, 20);
    brush_size_slider_->setValue(brush_size_);
    brush_size_slider_->setFixedWidth(100);
    brush_size_label_ = new QLabel(QString::number(brush_size_), tb);
    brush_size_label_->setFixedWidth(24);
    brush_size_label_->setAlignment(Qt::AlignCenter);
    tb->addWidget(brush_size_slider_);
    tb->addWidget(brush_size_label_);
    connect(brush_size_slider_, &QSlider::valueChanged,
            this, &TerrainEditWindow::onBrushSizeChanged);

    // Brush strength
    auto* str_lbl = new QLabel(tr("Str"), tb);
    str_lbl->setAlignment(Qt::AlignCenter);
    tb->addWidget(str_lbl);
    brush_strength_slider_ = new QSlider(Qt::Horizontal, tb);
    brush_strength_slider_->setRange(1, 100);
    brush_strength_slider_->setValue(static_cast<int>(brush_strength_ * 20.0f));
    brush_strength_slider_->setFixedWidth(100);
    brush_strength_label_ = new QLabel(QString::number(brush_strength_, 'f', 1), tb);
    brush_strength_label_->setFixedWidth(24);
    brush_strength_label_->setAlignment(Qt::AlignCenter);
    tb->addWidget(brush_strength_slider_);
    tb->addWidget(brush_strength_label_);
    connect(brush_strength_slider_, &QSlider::valueChanged,
            this, &TerrainEditWindow::onBrushStrengthChanged);

    // Texture selector
    tb->addSeparator();
    tb->addWidget(new QLabel(tr("Tex:"), tb));
    texture_combo_ = new QComboBox(tb);
    texture_combo_->setFixedWidth(100);
    texture_combo_->setEnabled(false);
    tb->addWidget(texture_combo_);
    connect(texture_combo_, &QComboBox::currentIndexChanged,
            this, &TerrainEditWindow::onTextureChanged);

    // Texture/height view toggle
    texture_toggle_btn_ = new QToolButton(tb);
    texture_toggle_btn_->setIcon(make_icon_texture());
    texture_toggle_btn_->setCheckable(true);
    texture_toggle_btn_->setToolTip(tr("Toggle texture color view"));
    tb->addWidget(texture_toggle_btn_);
    connect(texture_toggle_btn_, &QToolButton::toggled,
            this, &TerrainEditWindow::onShowTextureToggled);

    // Brush shape toggle (circle / square)
    brush_shape_btn_ = new QToolButton(tb);
    brush_shape_btn_->setIcon(make_icon_circle());
    brush_shape_btn_->setCheckable(true);
    brush_shape_btn_->setToolTip(tr("Toggle brush shape (circle / square)"));
    tb->addWidget(brush_shape_btn_);
    connect(brush_shape_btn_, &QToolButton::toggled, this, [this](bool checked) {
        BrushShape shape = checked ? BrushShape::Square : BrushShape::Circle;
        brush_shape_ = shape;
        brush_shape_btn_->setIcon(shape == BrushShape::Square ? make_icon_square() : make_icon_circle());
        widget_->setBrushShape(shape);
        status_label_->setText(shape == BrushShape::Square ? tr("Square brush") : tr("Circle brush"));
    });

    // Render mode selector
    tb->addSeparator();
    tb->addWidget(new QLabel(tr("Render:"), tb));
    render_mode_combo_ = new QComboBox(tb);
    render_mode_combo_->addItem(tr("Wireframe"));
    render_mode_combo_->addItem(tr("Unlit"));
    render_mode_combo_->addItem(tr("Lit"));
    render_mode_combo_->setCurrentIndex(2); // default Lit
    render_mode_combo_->setFixedWidth(80);
    tb->addWidget(render_mode_combo_);
    connect(render_mode_combo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
        RenderMode mode;
        switch (idx) {
        case 0:  mode = RenderMode::Wireframe; break;
        case 1:  mode = RenderMode::Unlit;     break;
        default: mode = RenderMode::Lit;       break;
        }
        widget_->setRenderMode(mode);
    });

    // Status label
    tb->addSeparator();
    status_label_ = new QLabel(tr("Ready"), tb);
    tb->addWidget(status_label_);

    // Fix toolbar width to prevent flickering as contents resize
    tb->setFixedWidth(tb->sizeHint().width() + 8);
}

// ============================================================
// Keyboard shortcuts
// ============================================================
void TerrainEditWindow::createShortcuts() {
    auto* sc1 = new QShortcut(QKeySequence(Qt::Key_1), this);
    connect(sc1, &QShortcut::activated, this, [this]{ selectTool(EditTool::Raise); });
    auto* sc2 = new QShortcut(QKeySequence(Qt::Key_2), this);
    connect(sc2, &QShortcut::activated, this, [this]{ selectTool(EditTool::Lower); });
    auto* sc3 = new QShortcut(QKeySequence(Qt::Key_3), this);
    connect(sc3, &QShortcut::activated, this, [this]{ selectTool(EditTool::Smooth); });
    auto* sc4 = new QShortcut(QKeySequence(Qt::Key_4), this);
    connect(sc4, &QShortcut::activated, this, [this]{ selectTool(EditTool::Flatten); });
    auto* sc5 = new QShortcut(QKeySequence(Qt::Key_5), this);
    connect(sc5, &QShortcut::activated, this, [this]{ selectTool(EditTool::Paint); });

    // Brush size: [ / ]
    auto* sc_br_down = new QShortcut(QKeySequence(Qt::Key_BracketLeft), this);
    connect(sc_br_down, &QShortcut::activated, this, [this]{
        int v = brush_size_slider_->value() - 1;
        brush_size_slider_->setValue(std::max(1, v));
    });
    auto* sc_br_up = new QShortcut(QKeySequence(Qt::Key_BracketRight), this);
    connect(sc_br_up, &QShortcut::activated, this, [this]{
        int v = brush_size_slider_->value() + 1;
        brush_size_slider_->setValue(std::min(20, v));
    });

    // Brush strength: Shift+[ / Shift+]
    auto* sc_st_down = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_BraceLeft), this);
    connect(sc_st_down, &QShortcut::activated, this, [this]{
        int v = brush_strength_slider_->value() - 5;
        brush_strength_slider_->setValue(std::max(1, v));
    });
    auto* sc_st_up = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_BraceRight), this);
    connect(sc_st_up, &QShortcut::activated, this, [this]{
        int v = brush_strength_slider_->value() + 5;
        brush_strength_slider_->setValue(std::min(100, v));
    });

    // Brush shape toggle: B
    auto* sc_shape = new QShortcut(QKeySequence(Qt::Key_B), this);
    connect(sc_shape, &QShortcut::activated, this, [this]{
        if (brush_shape_btn_) {
            brush_shape_btn_->toggle();
        }
    });

    // Undo / Redo: Ctrl+Z / Ctrl+Y
    auto* sc_undo = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z), this);
    connect(sc_undo, &QShortcut::activated, this, [this]{
        widget_->undo();
    });
    auto* sc_redo = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y), this);
    connect(sc_redo, &QShortcut::activated, this, [this]{
        widget_->redo();
    });

    // Focus on hovered tile: F
    auto* sc_focus = new QShortcut(QKeySequence(Qt::Key_F), this);
    connect(sc_focus, &QShortcut::activated, this, [this]{
        // Use the widget's hover state to focus
        widget_->focusOnTile(widget_->hoverCol(), widget_->hoverRow());
    });
}

// ============================================================
// Settings persistence
// ============================================================
void TerrainEditWindow::saveSettings() {
    QSettings qs;
    qs.beginGroup("TerrainEditWindow");
    qs.setValue("Geometry", saveGeometry());
    qs.setValue("Tool", static_cast<int>(current_tool_));
    qs.setValue("BrushSize", brush_size_);
    qs.setValue("BrushStrength", brush_strength_);
    qs.setValue("PaintTexture", paint_texture_);
    qs.setValue("ShowTexture", show_texture_);
    qs.setValue("BrushShape", static_cast<int>(brush_shape_));
    if (render_mode_combo_)
        qs.setValue("RenderMode", render_mode_combo_->currentIndex());
    qs.endGroup();
}

void TerrainEditWindow::restoreSettings() {
    QSettings qs;
    qs.beginGroup("TerrainEditWindow");

    auto geom = qs.value("Geometry").toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);

    int tool = qs.value("Tool", 0).toInt();
    if (tool >= 0 && tool <= 4)
        selectTool(static_cast<EditTool>(tool));

    brush_size_ = qs.value("BrushSize", 4).toInt();
    brush_size_ = qBound(1, brush_size_, 20);
    if (brush_size_slider_) brush_size_slider_->setValue(brush_size_);
    if (brush_size_label_) brush_size_label_->setText(QString::number(brush_size_));
    widget_->setBrushSize(brush_size_);

    float str = static_cast<float>(qs.value("BrushStrength", 1.0).toDouble());
    brush_strength_ = qBound(0.05f, str, 5.0f);
    if (brush_strength_slider_) brush_strength_slider_->setValue(static_cast<int>(brush_strength_ * 20.0f));
    if (brush_strength_label_) brush_strength_label_->setText(QString::number(brush_strength_, 'f', 1));
    widget_->setBrushStrength(brush_strength_);

    paint_texture_ = qs.value("PaintTexture", 0).toInt();
    show_texture_ = qs.value("ShowTexture", true).toBool();
    int render_idx = qs.value("RenderMode", 2).toInt();
    if (render_mode_combo_) {
        render_mode_combo_->setCurrentIndex(qBound(0, render_idx, 2));
    }
    // Sync texture toggle button with loaded value
    if (texture_toggle_btn_) {
        texture_toggle_btn_->setChecked(show_texture_);
    }

    int shape = qs.value("BrushShape", 0).toInt();
    brush_shape_ = (shape == 0) ? BrushShape::Circle : BrushShape::Square;
    if (brush_shape_btn_) {
        bool is_square = (brush_shape_ == BrushShape::Square);
        brush_shape_btn_->setChecked(is_square);
        brush_shape_btn_->setIcon(is_square ? make_icon_square() : make_icon_circle());
    }
    widget_->setBrushShape(brush_shape_);

    qs.endGroup();
}

void TerrainEditWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}

// ============================================================
// Tool selection helper
// ============================================================
void TerrainEditWindow::selectTool(EditTool tool) {
    onToolChanged(tool);
    QList<QToolButton*> btns = {raise_btn_, lower_btn_, smooth_btn_,
                                flatten_btn_, paint_btn_};
    int idx = static_cast<int>(tool);
    if (idx >= 0 && idx < btns.size() && btns[idx])
        btns[idx]->setChecked(true);
}

// ============================================================
// Map lifecycle
// ============================================================
void TerrainEditWindow::onMapLoaded(MapBuilder* builder) {
    try {
        terrain_ = builder->read_terrain();
        has_terrain_ = true;
        modified_ = false;
        widget_->setWc3DataDir(wc3_data_dir_);
        widget_->loadTerrain(&terrain_);
        widget_->setShowTexture(show_texture_);
        updateTextureCombo();

        // Compute statistics
        auto [min_it, max_it] = std::minmax_element(
            terrain_.tilepoints.begin(), terrain_.tilepoints.end(),
            [](const TilePoint& a, const TilePoint& b) { return a.height < b.height; });
        float min_h = min_it->height;
        float max_h = max_it->height;
        statusBar()->showMessage(
            tr("Loaded: %1 x %2 tiles | H: %3 ~ %4 (%5)")
                .arg(terrain_.tile_width - 1)
                .arg(terrain_.tile_height - 1)
                .arg(min_h, 0, 'f', 1)
                .arg(max_h, 0, 'f', 1)
                .arg(max_h - min_h, 0, 'f', 1));
    } catch (...) {
        onMapClosed();
    }
}

void TerrainEditWindow::onMapClosed() {
    has_terrain_ = false;
    modified_ = false;
    terrain_ = Terrain{};
    widget_->loadTerrain(nullptr);
    texture_combo_->clear();
    texture_combo_->setEnabled(false);
    statusBar()->showMessage(tr("No map loaded"));
}

void TerrainEditWindow::syncToBuilder(MapBuilder* builder) {
    if (!has_terrain_ || !builder) return;
    builder->set_terrain(terrain_);
    modified_ = false;
}

// ============================================================
// Slots
// ============================================================
void TerrainEditWindow::onTerrainEdited() {
    modified_ = true;
    status_label_->setText(tr("Terrain modified"));
    emit contentChanged();
}

void TerrainEditWindow::onToolChanged(EditTool tool) {
    current_tool_ = tool;
    widget_->setTool(tool);

    bool is_paint = (tool == EditTool::Paint);
    texture_combo_->setEnabled(is_paint);

    static const QString names[] = {
        tr("Raise"), tr("Lower"), tr("Smooth"),
        tr("Flatten"), tr("Paint")
    };
    int idx = static_cast<int>(tool);
    if (idx >= 0 && idx < 5)
        status_label_->setText(names[idx]);
}

void TerrainEditWindow::onBrushSizeChanged(int value) {
    brush_size_ = value;
    brush_size_label_->setText(QString::number(value));
    widget_->setBrushSize(value);
}

void TerrainEditWindow::onBrushStrengthChanged(int value) {
    brush_strength_ = value / 20.0f;
    brush_strength_label_->setText(QString::number(brush_strength_, 'f', 1));
    widget_->setBrushStrength(brush_strength_);
}

void TerrainEditWindow::onTextureChanged(int index) {
    if (index >= 0) {
        paint_texture_ = index;
        widget_->setPaintTexture(index);
    }
}

void TerrainEditWindow::onShowTextureToggled(bool checked) {
    show_texture_ = checked;
    widget_->setShowTexture(checked);
}

// ── Tile ID → Chinese name lookup ──
static QString tileDisplayName(const QString& id) {
    // Standard WC3 ground tile translations
    static const QMap<QString, QString> names = {
        // Lordaeron Summer (L)
        {"Ldrt", QStringLiteral("洛丹伦泥土")},
        {"Ldro", QStringLiteral("洛丹伦粗土")},
        {"Lrok", QStringLiteral("洛丹伦岩石")},
        {"Lgrs", QStringLiteral("洛丹伦草地")},
        {"Lgrh", QStringLiteral("洛丹伦高草")},
        {"Lpla", QStringLiteral("洛丹伦平草")},
        {"Lroc", QStringLiteral("洛丹伦荒地")},
        // Lordaeron Fall (F)
        {"Fdrt", QStringLiteral("洛丹伦秋季泥土")},
        {"Fdro", QStringLiteral("洛丹伦秋季粗土")},
        {"Frok", QStringLiteral("洛丹伦秋季岩石")},
        {"Fgrs", QStringLiteral("洛丹伦秋季草地")},
        {"Fgrh", QStringLiteral("洛丹伦秋季高草")},
        {"Fpla", QStringLiteral("洛丹伦秋季平草")},
        {"Froc", QStringLiteral("洛丹伦秋季荒地")},
        // Lordaeron Winter (W)
        {"Wdrt", QStringLiteral("洛丹伦冬季泥土")},
        {"Wdro", QStringLiteral("洛丹伦冬季粗土")},
        {"Wrok", QStringLiteral("洛丹伦冬季岩石")},
        {"Wgrs", QStringLiteral("洛丹伦冬季草地")},
        {"Wpla", QStringLiteral("洛丹伦冬季平草")},
        {"Whwa", QStringLiteral("洛丹伦冬雪")},
        {"Wroc", QStringLiteral("洛丹伦冬季荒地")},
        // Barrens (B)
        {"Bdrt", QStringLiteral("贫瘠之地泥土")},
        {"Bdro", QStringLiteral("贫瘠之地粗土")},
        {"Brok", QStringLiteral("贫瘠之地岩石")},
        {"Bgrs", QStringLiteral("贫瘠之地草地")},
        {"Bgrh", QStringLiteral("贫瘠之地高草")},
        {"Bpla", QStringLiteral("贫瘠之地平草")},
        {"Bdsr", QStringLiteral("贫瘠之地沙漠")},
        // Ashenvale (A)
        {"Adrt", QStringLiteral("灰谷泥土")},
        {"Adro", QStringLiteral("灰谷粗土")},
        {"Arok", QStringLiteral("灰谷岩石")},
        {"Agrs", QStringLiteral("灰谷草地")},
        {"Agrh", QStringLiteral("灰谷高草")},
        {"Apla", QStringLiteral("灰谷平草")},
        {"Ados", QStringLiteral("灰谷密林")},
        // Felwood (C)
        {"Cdrt", QStringLiteral("费尔伍德泥土")},
        {"Cdro", QStringLiteral("费尔伍德粗土")},
        {"Crok", QStringLiteral("费尔伍德岩石")},
        {"Cgrs", QStringLiteral("费尔伍德草地")},
        {"Cgrh", QStringLiteral("费尔伍德高草")},
        {"Cpla", QStringLiteral("费尔伍德平草")},
        {"Cdos", QStringLiteral("费尔伍德密林")},
        // Northrend (N)
        {"Ndrt", QStringLiteral("诺森德泥土")},
        {"Ndro", QStringLiteral("诺森德粗土")},
        {"Nrok", QStringLiteral("诺森德岩石")},
        {"Ngrs", QStringLiteral("诺森德草地")},
        {"Ngrh", QStringLiteral("诺森德高草")},
        {"Nsnw", QStringLiteral("诺森德雪地")},
        // Cityscape (Y) — urban
        {"Ydrt", QStringLiteral("城市泥土")},
        {"Ywtf", QStringLiteral("城市人行道")},
        {"Ygtf", QStringLiteral("城市人行道(带花纹)")},
        {"Yhdg", QStringLiteral("城市树篱")},
        {"Yblp", QStringLiteral("城市黑砖")},
        {"Ysqd", QStringLiteral("城市广场")},
        {"Ysor", QStringLiteral("城市圆石")},
        {"Ybbb", QStringLiteral("城市破损砖")},
        {"Yclf", QStringLiteral("城市悬崖")},
        {"Yrok", QStringLiteral("城市岩石")},
        {"Ydsr", QStringLiteral("城市沙漠")},
        {"Ygrs", QStringLiteral("城市草地")},
        // Underground (G) — caves
        {"Gdrt", QStringLiteral("地下泥土")},
        {"Gdro", QStringLiteral("地下粗土")},
        {"Grok", QStringLiteral("地下岩石")},
        {"Ggrs", QStringLiteral("地下草地")},
        {"Gpla", QStringLiteral("地下平草")},
        // Dungeon (D)
        {"Ddrt", QStringLiteral("地牢泥土")},
        {"Ddro", QStringLiteral("地牢粗土")},
        {"Drok", QStringLiteral("地牢岩石")},
        {"Dgrs", QStringLiteral("地牢草地")},
        {"Dpla", QStringLiteral("地牢平草")},
        // Dalaran (X)
        {"Xdrt", QStringLiteral("达拉然泥土")},
        {"Xwtw", QStringLiteral("达拉然白色砖")},
        {"Xbbb", QStringLiteral("达拉然破损砖")},
        {"Xhdg", QStringLiteral("达拉然树篱")},
        {"Xclf", QStringLiteral("达拉然悬崖")},
        {"Xsqd", QStringLiteral("达拉然广场")},
        {"Xblp", QStringLiteral("达拉然黑砖")},
        // Sunken Ruins (Z)
        {"Zdrt", QStringLiteral("沉没废墟泥土")},
        {"Zblp", QStringLiteral("沉没废墟黑砖")},
        {"Zclf", QStringLiteral("沉没废墟悬崖")},
        {"Zsqd", QStringLiteral("沉没废墟广场")},
        {"Zbbb", QStringLiteral("沉没废墟破损砖")},
        // Icecrown (I)
        {"Idrt", QStringLiteral("冰冠泥土")},
        {"Iroc", QStringLiteral("冰冠岩石")},
        {"Isnw", QStringLiteral("冰冠雪地")},
        // Dalaran Ruins (J)
        {"Jdrt", QStringLiteral("达拉然废墟泥土")},
        {"Jrok", QStringLiteral("达拉然废墟岩石")},
        {"Jsor", QStringLiteral("达拉然废墟圆石")},
        {"Jgrs", QStringLiteral("达拉然废墟草地")},
        // Outland (O)
        {"Odrt", QStringLiteral("外域泥土")},
        {"Orok", QStringLiteral("外域岩石")},
        {"Ogrs", QStringLiteral("外域草地")},
        {"Opla", QStringLiteral("外域平草")},
        {"Odos", QStringLiteral("外域密林")},
        // Black Citadel (K)
        {"Kdrt", QStringLiteral("黑色城堡泥土")},
        {"Krok", QStringLiteral("黑色城堡岩石")},
        {"Kgrs", QStringLiteral("黑色城堡草地")},
        {"Kpla", QStringLiteral("黑色城堡平草")},
        // Village (V) / Village Fall (Q)
        {"Vdrt", QStringLiteral("村庄泥土")},
        {"Vdro", QStringLiteral("村庄粗土")},
        {"Vrok", QStringLiteral("村庄岩石")},
        {"Vgrs", QStringLiteral("村庄草地")},
        {"Vgrh", QStringLiteral("村庄高草")},
        {"Vpla", QStringLiteral("村庄平草")},
        {"Qdrt", QStringLiteral("秋季村庄泥土")},
        {"Qdro", QStringLiteral("秋季村庄粗土")},
        {"Qrok", QStringLiteral("秋季村庄岩石")},
        {"Qgrs", QStringLiteral("秋季村庄草地")},
        {"Qgrh", QStringLiteral("秋季村庄高草")},
        {"Qpla", QStringLiteral("秋季村庄平草")},
    };

    auto it = names.find(id);
    if (it != names.end())
        return it.value();
    return id;  // fallback: show raw ID
}

void TerrainEditWindow::updateTextureCombo() {
    texture_combo_->blockSignals(true);
    texture_combo_->clear();

    for (size_t i = 0; i < terrain_.ground_tiles.size(); ++i) {
        QString id = QString::fromStdString(terrain_.ground_tiles[i]);
        texture_combo_->addItem(QString("%1: %2").arg(i).arg(tileDisplayName(id)));
    }

    if (!terrain_.ground_tiles.empty()) {
        texture_combo_->setEnabled(current_tool_ == EditTool::Paint);
        texture_combo_->setCurrentIndex(qBound(0, paint_texture_,
            static_cast<int>(terrain_.ground_tiles.size()) - 1));
        widget_->setPaintTexture(paint_texture_);
    }

    texture_combo_->blockSignals(false);
}
