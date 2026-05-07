#include "mapinfopage.h"
#include "../src/core/types.h"
#include "../src/core/map_builder.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QHeaderView>

// ============================================================
// Helpers
// ============================================================
static QDoubleSpinBox* make_double_spin(QWidget* parent, double def, double min, double max) {
    auto* s = new QDoubleSpinBox(parent);
    s->setRange(min, max);
    s->setValue(def);
    s->setDecimals(4);
    return s;
}

static QSpinBox* make_spin(QWidget* parent, int def, int min, int max) {
    auto* s = new QSpinBox(parent);
    s->setRange(min, max);
    s->setValue(def);
    return s;
}

// ============================================================
// Constructor
// ============================================================
MapInfoPage::MapInfoPage(QWidget* parent)
    : QWidget(parent)
{
    build_ui();
}

void MapInfoPage::build_ui() {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);

    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);

    layout->addWidget(create_basic_group());
    layout->addWidget(create_loading_group());
    layout->addWidget(create_prologue_group());
    layout->addWidget(create_environment_group());
    layout->addWidget(create_players_group());
    layout->addWidget(create_forces_group());

    scroll->setWidget(container);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->addWidget(scroll);

    // Wire change signals to contentChanged()
    connect(name_edit_, &QLineEdit::textChanged, this, [this]() { if (!loading_) emit contentChanged(); });
    connect(author_edit_, &QLineEdit::textChanged, this, [this]() { if (!loading_) emit contentChanged(); });
    connect(desc_edit_, &QTextEdit::textChanged, this, [this]() { if (!loading_) emit contentChanged(); });
    connect(players_desc_edit_, &QLineEdit::textChanged, this, [this]() { if (!loading_) emit contentChanged(); });
    connect(map_version_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(map_width_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(map_height_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(cam_left_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(cam_right_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(cam_bottom_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(cam_top_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(loading_type_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(loading_path_, &QLineEdit::textChanged, this, [this]() { if (!loading_) emit contentChanged(); });
    connect(loading_text_, &QTextEdit::textChanged, this, [this]() { if (!loading_) emit contentChanged(); });
    connect(loading_title_, &QLineEdit::textChanged, this, [this]() { if (!loading_) emit contentChanged(); });
    connect(loading_subtitle_, &QLineEdit::textChanged, this, [this]() { if (!loading_) emit contentChanged(); });
    connect(prologue_path_, &QLineEdit::textChanged, this, [this]() { if (!loading_) emit contentChanged(); });
    connect(prologue_text_, &QTextEdit::textChanged, this, [this]() { if (!loading_) emit contentChanged(); });
    connect(prologue_title_, &QLineEdit::textChanged, this, [this]() { if (!loading_) emit contentChanged(); });
    connect(prologue_subtitle_, &QLineEdit::textChanged, this, [this]() { if (!loading_) emit contentChanged(); });
    connect(fog_type_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(fog_start_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(fog_end_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(fog_density_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(fog_r_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(fog_g_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(fog_b_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(water_r_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(water_g_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(water_b_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(script_type_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { if (!loading_) emit contentChanged(); });
    connect(players_table_, &QTableWidget::itemChanged, this, [this]() { if (!loading_) emit contentChanged(); });
    connect(forces_table_, &QTableWidget::itemChanged, this, [this]() { if (!loading_) emit contentChanged(); });
}

QWidget* MapInfoPage::create_basic_group() {
    auto* g = new QGroupBox(tr("Basic Info"));
    auto* f = new QFormLayout(g);

    name_edit_ = new QLineEdit;
    author_edit_ = new QLineEdit;
    desc_edit_ = new QTextEdit;
    desc_edit_->setMaximumHeight(80);
    players_desc_edit_ = new QLineEdit;
    map_version_spin_ = make_spin(this, 1, 0, 99999);
    map_width_spin_ = make_spin(this, 64, 16, 512);
    map_height_spin_ = make_spin(this, 64, 16, 512);

    f->addRow(tr("Map Name:"), name_edit_);
    f->addRow(tr("Author:"), author_edit_);
    f->addRow(tr("Description:"), desc_edit_);
    f->addRow(tr("Players Desc:"), players_desc_edit_);
    f->addRow(tr("Map Version:"), map_version_spin_);
    f->addRow(tr("Width:"), map_width_spin_);
    f->addRow(tr("Height:"), map_height_spin_);

    // Camera Bounds (inline in Basic Info to reduce visual weight)
    auto* cam_header = new QLabel(tr("Camera Bounds"));
    cam_header->setStyleSheet("font-weight: bold; margin-top: 6px");
    f->addRow(cam_header);

    cam_left_ = make_double_spin(this, 0, -99999, 99999);
    cam_right_ = make_double_spin(this, 0, -99999, 99999);
    cam_bottom_ = make_double_spin(this, 0, -99999, 99999);
    cam_top_ = make_double_spin(this, 0, -99999, 99999);
    f->addRow(tr("Left:"), cam_left_);
    f->addRow(tr("Right:"), cam_right_);
    f->addRow(tr("Bottom:"), cam_bottom_);
    f->addRow(tr("Top:"), cam_top_);

    return g;
}

QWidget* MapInfoPage::create_loading_group() {
    auto* g = new QGroupBox(tr("Loading Screen"));
    auto* f = new QFormLayout(g);

    loading_type_ = new QComboBox;
    loading_type_->addItems({tr("None"), tr("Linear"), tr("Exponential")});

    loading_path_ = new QLineEdit;
    loading_text_ = new QTextEdit;
    loading_text_->setMaximumHeight(60);
    loading_title_ = new QLineEdit;
    loading_subtitle_ = new QLineEdit;

    f->addRow(tr("Type:"), loading_type_);
    f->addRow(tr("Path:"), loading_path_);
    f->addRow(tr("Text:"), loading_text_);
    f->addRow(tr("Title:"), loading_title_);
    f->addRow(tr("Subtitle:"), loading_subtitle_);

    return g;
}

QWidget* MapInfoPage::create_prologue_group() {
    auto* g = new QGroupBox(tr("Prologue"));
    auto* f = new QFormLayout(g);

    prologue_path_ = new QLineEdit;
    prologue_text_ = new QTextEdit;
    prologue_text_->setMaximumHeight(60);
    prologue_title_ = new QLineEdit;
    prologue_subtitle_ = new QLineEdit;

    f->addRow(tr("Path:"), prologue_path_);
    f->addRow(tr("Text:"), prologue_text_);
    f->addRow(tr("Title:"), prologue_title_);
    f->addRow(tr("Subtitle:"), prologue_subtitle_);

    return g;
}

QWidget* MapInfoPage::create_environment_group() {
    auto* g = new QGroupBox(tr("Environment"));
    auto* f = new QFormLayout(g);

    fog_type_ = new QComboBox;
    fog_type_->addItems({tr("None"), tr("Linear"), tr("Exponential")});

    fog_start_ = make_double_spin(this, 0, 0, 99999);
    fog_end_ = make_double_spin(this, 0, 0, 99999);
    fog_density_ = make_double_spin(this, 0, 0, 99999);

    fog_r_ = make_spin(this, 0, 0, 255);
    fog_g_ = make_spin(this, 0, 0, 255);
    fog_b_ = make_spin(this, 0, 0, 255);

    water_r_ = make_spin(this, 0, 0, 255);
    water_g_ = make_spin(this, 0, 0, 255);
    water_b_ = make_spin(this, 0, 0, 255);

    script_type_ = new QComboBox;
    script_type_->addItems({tr("JASS"), tr("Lua")});

    f->addRow(tr("Fog Type:"), fog_type_);
    f->addRow(tr("Fog Start Z:"), fog_start_);
    f->addRow(tr("Fog End Z:"), fog_end_);
    f->addRow(tr("Fog Density:"), fog_density_);
    f->addRow(tr("Fog Color R:"), fog_r_);
    f->addRow(tr("Fog Color G:"), fog_g_);
    f->addRow(tr("Fog Color B:"), fog_b_);

    f->addRow(tr("Water R:"), water_r_);
    f->addRow(tr("Water G:"), water_g_);
    f->addRow(tr("Water B:"), water_b_);

    f->addRow(tr("Script:"), script_type_);

    return g;
}

QWidget* MapInfoPage::create_players_group() {
    auto* g = new QGroupBox(tr("Players"));
    auto* layout = new QVBoxLayout(g);

    players_table_ = new QTableWidget(0, 6, this);
    players_table_->setHorizontalHeaderLabels({
        tr("ID"), tr("Type"), tr("Race"), tr("Fixed"), tr("Name"), tr("Ally")
    });
    players_table_->horizontalHeader()->setStretchLastSection(true);
    players_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    players_table_->setMinimumHeight(120);

    auto* btn_row = new QHBoxLayout;
    auto* add_btn = new QPushButton(tr("+"));
    auto* remove_btn = new QPushButton(tr("-"));
    btn_row->addWidget(add_btn);
    btn_row->addWidget(remove_btn);
    btn_row->addStretch();

    connect(add_btn, &QPushButton::clicked, [this]() {
        int row = players_table_->rowCount();
        players_table_->insertRow(row);
        players_table_->setItem(row, 0, new QTableWidgetItem("0"));
        players_table_->setItem(row, 4, new QTableWidgetItem("Player"));
        if (!loading_) emit contentChanged();
    });

    connect(remove_btn, &QPushButton::clicked, [this]() {
        int row = players_table_->currentRow();
        if (row >= 0) players_table_->removeRow(row);
        if (!loading_) emit contentChanged();
    });

    layout->addWidget(players_table_);
    layout->addLayout(btn_row);

    return g;
}

QWidget* MapInfoPage::create_forces_group() {
    auto* g = new QGroupBox(tr("Forces"));
    auto* layout = new QVBoxLayout(g);

    forces_table_ = new QTableWidget(0, 3, this);
    forces_table_->setHorizontalHeaderLabels({
        tr("Flags"), tr("Player Mask"), tr("Name")
    });
    forces_table_->horizontalHeader()->setStretchLastSection(true);
    forces_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    forces_table_->setMinimumHeight(100);

    auto* btn_row = new QHBoxLayout;
    auto* add_btn = new QPushButton(tr("+"));
    auto* remove_btn = new QPushButton(tr("-"));
    btn_row->addWidget(add_btn);
    btn_row->addWidget(remove_btn);
    btn_row->addStretch();

    connect(add_btn, &QPushButton::clicked, [this]() {
        int row = forces_table_->rowCount();
        forces_table_->insertRow(row);
        forces_table_->setItem(row, 2, new QTableWidgetItem("Force"));
        if (!loading_) emit contentChanged();
    });

    connect(remove_btn, &QPushButton::clicked, [this]() {
        int row = forces_table_->currentRow();
        if (row >= 0) forces_table_->removeRow(row);
        if (!loading_) emit contentChanged();
    });

    layout->addWidget(forces_table_);
    layout->addLayout(btn_row);

    return g;
}

// ============================================================
// Load
// ============================================================
void MapInfoPage::load(const MapInfo& info) {
    loading_ = true;
    clear();

    name_edit_->setText(QString::fromStdString(info.map_name));
    author_edit_->setText(QString::fromStdString(info.author));
    desc_edit_->setPlainText(QString::fromStdString(info.description));
    players_desc_edit_->setText(QString::fromStdString(info.players_desc));
    map_version_spin_->setValue(info.map_version);
    map_width_spin_->setValue(info.map_width);
    map_height_spin_->setValue(info.map_height);

    cam_left_->setValue(info.camera_bounds.left);
    cam_right_->setValue(info.camera_bounds.right);
    cam_bottom_->setValue(info.camera_bounds.bottom);
    cam_top_->setValue(info.camera_bounds.top);

    loading_type_->setCurrentIndex(info.loading_screen_id);
    loading_path_->setText(QString::fromStdString(info.loading_screen_path));
    loading_text_->setPlainText(QString::fromStdString(info.loading_screen_text));
    loading_title_->setText(QString::fromStdString(info.loading_screen_title));
    loading_subtitle_->setText(QString::fromStdString(info.loading_screen_subtitle));

    prologue_path_->setText(QString::fromStdString(info.prologue_path));
    prologue_text_->setPlainText(QString::fromStdString(info.prologue_text));
    prologue_title_->setText(QString::fromStdString(info.prologue_title));
    prologue_subtitle_->setText(QString::fromStdString(info.prologue_subtitle));

    fog_type_->setCurrentIndex(info.fog.type);
    fog_start_->setValue(info.fog.start_z);
    fog_end_->setValue(info.fog.end_z);
    fog_density_->setValue(info.fog.density);
    fog_r_->setValue(info.fog.r);
    fog_g_->setValue(info.fog.g);
    fog_b_->setValue(info.fog.b);

    water_r_->setValue(info.water_r);
    water_g_->setValue(info.water_g);
    water_b_->setValue(info.water_b);

    script_type_->setCurrentIndex(info.script_type);

    load_players(info.players);
    load_forces(info.forces);
    loading_ = false;
}

void MapInfoPage::load_players(const std::vector<PlayerInfo>& players) {
    players_table_->setRowCount((int)players.size());
    for (int i = 0; i < (int)players.size(); ++i) {
        auto& p = players[i];
        players_table_->setItem(i, 0, new QTableWidgetItem(QString::number(p.id)));
        players_table_->setItem(i, 1, new QTableWidgetItem(QString::number(p.type)));
        players_table_->setItem(i, 2, new QTableWidgetItem(QString::number(p.race)));
        players_table_->setItem(i, 3, new QTableWidgetItem(QString::number(p.fixed_start)));
        players_table_->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(p.name)));
        players_table_->setItem(i, 5, new QTableWidgetItem(QString::number(p.ally_low)));
    }
}

void MapInfoPage::load_forces(const std::vector<ForceInfo>& forces) {
    forces_table_->setRowCount((int)forces.size());
    for (int i = 0; i < (int)forces.size(); ++i) {
        auto& f = forces[i];
        forces_table_->setItem(i, 0, new QTableWidgetItem(QString::number(f.flags)));
        forces_table_->setItem(i, 1, new QTableWidgetItem(QString::number(f.player_mask)));
        forces_table_->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(f.name)));
    }
}

// ============================================================
// Collect — overrides UI-visible fields on top of existing MapInfo
// ============================================================
void MapInfoPage::collect_into(MapInfo& info) const {

    info.map_name = name_edit_->text().toStdString();
    info.author = author_edit_->text().toStdString();
    info.description = desc_edit_->toPlainText().toStdString();
    info.players_desc = players_desc_edit_->text().toStdString();
    info.map_version = map_version_spin_->value();
    info.map_width = map_width_spin_->value();
    info.map_height = map_height_spin_->value();

    info.camera_bounds.left = (float)cam_left_->value();
    info.camera_bounds.right = (float)cam_right_->value();
    info.camera_bounds.bottom = (float)cam_bottom_->value();
    info.camera_bounds.top = (float)cam_top_->value();

    info.loading_screen_id = loading_type_->currentIndex();
    info.loading_screen_path = loading_path_->text().toStdString();
    info.loading_screen_text = loading_text_->toPlainText().toStdString();
    info.loading_screen_title = loading_title_->text().toStdString();
    info.loading_screen_subtitle = loading_subtitle_->text().toStdString();

    info.prologue_path = prologue_path_->text().toStdString();
    info.prologue_text = prologue_text_->toPlainText().toStdString();
    info.prologue_title = prologue_title_->text().toStdString();
    info.prologue_subtitle = prologue_subtitle_->text().toStdString();

    info.fog.type = fog_type_->currentIndex();
    info.fog.start_z = (float)fog_start_->value();
    info.fog.end_z = (float)fog_end_->value();
    info.fog.density = (float)fog_density_->value();
    info.fog.r = (uint8_t)fog_r_->value();
    info.fog.g = (uint8_t)fog_g_->value();
    info.fog.b = (uint8_t)fog_b_->value();

    info.water_r = (uint8_t)water_r_->value();
    info.water_g = (uint8_t)water_g_->value();
    info.water_b = (uint8_t)water_b_->value();

    info.script_type = script_type_->currentIndex();

    info.players = collect_players();
    info.forces = collect_forces();
}

std::vector<PlayerInfo> MapInfoPage::collect_players() const {
    std::vector<PlayerInfo> players;
    int rows = players_table_->rowCount();
    players.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        PlayerInfo p;
        auto* id_item = players_table_->item(i, 0);
        auto* type_item = players_table_->item(i, 1);
        auto* race_item = players_table_->item(i, 2);
        auto* fixed_item = players_table_->item(i, 3);
        auto* name_item = players_table_->item(i, 4);
        auto* ally_item = players_table_->item(i, 5);

        p.id = id_item ? id_item->text().toInt() : 0;
        p.type = type_item ? type_item->text().toInt() : 0;
        p.race = race_item ? race_item->text().toInt() : 0;
        p.fixed_start = fixed_item ? fixed_item->text().toInt() : 0;
        p.name = name_item ? name_item->text().toStdString() : "";
        p.ally_low = ally_item ? ally_item->text().toInt() : 0;
        players.push_back(p);
    }
    return players;
}

std::vector<ForceInfo> MapInfoPage::collect_forces() const {
    std::vector<ForceInfo> forces;
    int rows = forces_table_->rowCount();
    forces.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        ForceInfo f;
        auto* flags_item = forces_table_->item(i, 0);
        auto* mask_item = forces_table_->item(i, 1);
        auto* name_item = forces_table_->item(i, 2);

        f.flags = flags_item ? flags_item->text().toInt() : 0;
        f.player_mask = mask_item ? mask_item->text().toInt() : 0;
        f.name = name_item ? name_item->text().toStdString() : "";
        forces.push_back(f);
    }
    return forces;
}

// ============================================================
// Clear
// ============================================================
void MapInfoPage::clear() {
    name_edit_->clear();
    author_edit_->clear();
    desc_edit_->clear();
    players_desc_edit_->clear();
    map_version_spin_->setValue(1);
    map_width_spin_->setValue(64);
    map_height_spin_->setValue(64);

    cam_left_->setValue(0);
    cam_right_->setValue(0);
    cam_bottom_->setValue(0);
    cam_top_->setValue(0);

    loading_type_->setCurrentIndex(0);
    loading_path_->clear();
    loading_text_->clear();
    loading_title_->clear();
    loading_subtitle_->clear();

    prologue_path_->clear();
    prologue_text_->clear();
    prologue_title_->clear();
    prologue_subtitle_->clear();

    fog_type_->setCurrentIndex(0);
    fog_start_->setValue(0);
    fog_end_->setValue(0);
    fog_density_->setValue(0);
    fog_r_->setValue(0);
    fog_g_->setValue(0);
    fog_b_->setValue(0);

    water_r_->setValue(0);
    water_g_->setValue(0);
    water_b_->setValue(0);

    script_type_->setCurrentIndex(0);

    players_table_->setRowCount(0);
    forces_table_->setRowCount(0);
}

// ============================================================
// MapInfoPlugin
// ============================================================
bool MapInfoPlugin::init(PluginContext& ctx) {
    builder_ = ctx.builder;
    page_ = new MapInfoPage(ctx.parent_widget);
    connect(page_, &MapInfoPage::contentChanged, this, &MapInfoPlugin::contentChanged);
    return true;
}

void MapInfoPlugin::activate() {
    if (!page_ || !builder_) return;
    try {
        auto info = builder_->read_w3i();
        page_->load(info);
    } catch (...) {
        page_->clear();
    }
}

void MapInfoPlugin::deactivate() {
    if (page_) page_->clear();
}

void MapInfoPlugin::sync_to_builder() {
    if (!page_ || !builder_) return;
    auto info = builder_->read_w3i();
    page_->collect_into(info);
    builder_->set_w3i(info);
}

void MapInfoPlugin::destroy() {
    delete page_;
    page_ = nullptr;
    builder_ = nullptr;
}
