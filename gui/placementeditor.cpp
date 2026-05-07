#include "placementeditor.h"
#include "../src/core/map_builder.h"
#include "../src/core/metadata.h"
#include <QToolTip>
#include <QMouseEvent>

// Player colors matching WC3
static const QColor kPlayerColors[] = {
    QColor(0, 66, 255),    // 0  Red
    QColor(0, 192, 0),     // 1  Blue
    QColor(252, 220, 0),   // 2  Teal
    QColor(32, 96, 0),     // 3  Purple
    QColor(255, 0, 0),     // 4  Yellow
    QColor(255, 152, 0),   // 5  Orange
    QColor(32, 192, 192),  // 6  Green
    QColor(128, 0, 128),   // 7  Pink
    QColor(160, 96, 0),    // 8  Gray
    QColor(128, 128, 160), // 9  Light Blue
    QColor(64, 64, 64),    // 10 Dark Green
    QColor(128, 64, 0),    // 11 Brown
    QColor(0, 0, 0),       // 12 Black
    QColor(112, 48, 192),  // 13 Dark Blue (custom)
    QColor(96, 96, 96),    // 14 Dark Gray
    QColor(64, 192, 255),  // 15 Light Yellow (custom)
    QColor(192, 192, 192), // 16 Neutral Passive (light gray)
};

// ============================================================
// PlacementWidget
// ============================================================
PlacementWidget::PlacementWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(200, 200);
}

void PlacementWidget::load_data(MapBuilder* builder) {
    units_.clear();
    has_data_ = false;

    if (!builder) return;

    try {
        // Get terrain bounds
        auto terrain = builder->read_terrain();
        float tile_size = 128.0f;
        world_min_x_ = terrain.center_offset_x;
        world_min_y_ = terrain.center_offset_y;
        world_max_x_ = world_min_x_ + (terrain.tile_width - 1) * tile_size;
        world_max_y_ = world_min_y_ + (terrain.tile_height - 1) * tile_size;
        has_data_ = true;
    } catch (...) {
        // No terrain data; use unit bounds
        world_min_x_ = world_min_y_ = -99999;
        world_max_x_ = world_max_y_ = 99999;
    }

    // Load units
    try {
        auto units = builder->read_placed_units();
        for (auto& u : units.units) {
            UnitInfo info;
            info.x = u.x;
            info.y = u.y;
            info.type_id = u.type_id;
            info.owner = u.owner;
            info.is_doodad = false;
            units_.push_back(info);

            // Expand bounds if no terrain
            if (!has_data_) {
                if (u.x < world_min_x_) world_min_x_ = u.x;
                if (u.x > world_max_x_) world_max_x_ = u.x;
                if (u.y < world_min_y_) world_min_y_ = u.y;
                if (u.y > world_max_y_) world_max_y_ = u.y;
            }
        }
    } catch (...) {}

    // Load doodads
    try {
        auto doodads = builder->read_placed_doodads();
        for (auto& d : doodads.doodads) {
            UnitInfo info;
            info.x = d.x;
            info.y = d.y;
            info.type_id = d.type_id;
            info.owner = -1; // doodads have no player owner
            info.is_doodad = true;
            units_.push_back(info);
        }
    } catch (...) {}

    // Prevent division by zero
    float w = world_max_x_ - world_min_x_;
    float h = world_max_y_ - world_min_y_;
    if (w < 1.0f) { w = 1.0f; world_max_x_ = world_min_x_ + 1.0f; }
    if (h < 1.0f) { h = 1.0f; world_max_y_ = world_min_y_ + 1.0f; }

    update();
}

void PlacementWidget::clear_data() {
    units_.clear();
    has_data_ = false;
    update();
}

QPointF PlacementWidget::world_to_screen(float wx, float wy) const {
    if (!has_data_) return QPointF(margin_, margin_);
    float sx = margin_ + (wx - world_min_x_) * scale_;
    float sy = height() - margin_ - (wy - world_min_y_) * scale_; // flip Y
    return QPointF(sx, sy);
}

QColor PlacementWidget::player_color(int owner) const {
    if (owner < 0 || owner >= 24) return QColor(160, 160, 160);
    if (owner < 16) return kPlayerColors[owner];
    if (owner == 16) return kPlayerColors[16]; // Neutral Passive
    // 17-23: extra players, cycle through first 8
    return kPlayerColors[owner % 8];
}

void PlacementWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor(40, 40, 45));

    if (!has_data_ && units_.empty()) {
        painter.setPen(QColor(128, 128, 128));
        painter.drawText(rect(), Qt::AlignCenter, tr("Open a map to view placement"));
        return;
    }

    // Compute scale to fit in widget
    float area_w = static_cast<float>(width() - 2 * margin_);
    float area_h = static_cast<float>(height() - 2 * margin_);
    float world_w = world_max_x_ - world_min_x_;
    float world_h = world_max_y_ - world_min_y_;
    scale_ = qMin(area_w / world_w, area_h / world_h);

    // Draw grid (every 128 units = 1 tile)
    float grid_step = 128.0f * scale_;
    if (grid_step > 4.0f) {
        painter.setPen(QPen(QColor(60, 60, 70), 1));
        for (float gx = world_min_x_; gx <= world_max_x_; gx += 128.0f) {
            auto p = world_to_screen(gx, world_min_y_);
            auto p2 = world_to_screen(gx, world_max_y_);
            painter.drawLine(QPointF(p.x(), p.y()), QPointF(p2.x(), p2.y()));
        }
        for (float gy = world_min_y_; gy <= world_max_y_; gy += 128.0f) {
            auto p = world_to_screen(world_min_x_, gy);
            auto p2 = world_to_screen(world_max_x_, gy);
            painter.drawLine(QPointF(p.x(), p.y()), QPointF(p2.x(), p2.y()));
        }
    }

    // Draw units
    float dot_radius = qMax(3.0f, scale_ * 4.0f);
    for (size_t i = 0; i < units_.size(); ++i) {
        auto& u = units_[i];
        QPointF pos = world_to_screen(u.x, u.y);

        QColor color = u.is_doodad
            ? QColor(140, 140, 140)
            : player_color(u.owner);

        if (static_cast<int>(i) == hovered_idx_) {
            painter.setBrush(color.lighter(150));
            painter.setPen(QPen(Qt::white, 2));
            painter.drawEllipse(pos, dot_radius + 2, dot_radius + 2);
        } else {
            painter.setBrush(color);
            painter.setPen(QPen(color.darker(150), 1));
            painter.drawEllipse(pos, dot_radius, dot_radius);
        }
    }
}

void PlacementWidget::mouseMoveEvent(QMouseEvent* event) {
    if (units_.empty()) return;

    // Find nearest unit within hit radius
    float hit_radius = qMax(5.0f, scale_ * 5.0f);
    int nearest = -1;
    float nearest_dist = hit_radius * hit_radius;

    for (size_t i = 0; i < units_.size(); ++i) {
        QPointF pos = world_to_screen(units_[i].x, units_[i].y);
        float dx = static_cast<float>(event->position().x() - pos.x());
        float dy = static_cast<float>(event->position().y() - pos.y());
        float dist = dx * dx + dy * dy;
        if (dist < nearest_dist) {
            nearest_dist = dist;
            nearest = static_cast<int>(i);
        }
    }

    if (nearest != hovered_idx_) {
        hovered_idx_ = nearest;
        update();
    }

    // Show tooltip
    if (nearest >= 0) {
        auto& u = units_[nearest];
        QString type_id = QString::fromStdString(u.type_id);
        QString type_name;
        if (meta_db_) {
            auto* name = meta_db_->find_object_name(u.type_id);
            if (name)
                type_name = QString::fromStdString(*name);
        }
        QString type_label = type_name.isEmpty()
            ? type_id
            : QString("%1 (%2)").arg(type_name).arg(type_id);

        QString tip;
        if (u.is_doodad) {
            tip = tr("Doodad: %1\n(X: %2, Y: %3)")
                .arg(type_label)
                .arg(u.x, 0, 'f', 1)
                .arg(u.y, 0, 'f', 1);
        } else {
            QString owner_name = u.owner == 16 ? tr("Neutral") : tr("Player %1").arg(u.owner + 1);
            tip = tr("Unit: %1\nOwner: %2\n(X: %3, Y: %4)")
                .arg(type_label)
                .arg(owner_name)
                .arg(u.x, 0, 'f', 1)
                .arg(u.y, 0, 'f', 1);
        }
        QToolTip::showText(event->globalPosition().toPoint(), tip, this);
    } else {
        QToolTip::hideText();
    }
}

void PlacementWidget::leaveEvent(QEvent*) {
    if (hovered_idx_ >= 0) {
        hovered_idx_ = -1;
        update();
    }
}

// ============================================================
// PlacementEditorPlugin
// ============================================================
PlacementEditorPlugin::PlacementEditorPlugin() = default;
PlacementEditorPlugin::~PlacementEditorPlugin() = default;

bool PlacementEditorPlugin::init(PluginContext& ctx) {
    builder_ = ctx.builder;
    widget_ = new PlacementWidget(ctx.parent_widget);
    widget_->set_meta_db(ctx.meta_db);
    return true;
}

void PlacementEditorPlugin::activate() {
    if (widget_ && builder_)
        widget_->load_data(builder_);
}

void PlacementEditorPlugin::deactivate() {
    if (widget_)
        widget_->clear_data();
}

void PlacementEditorPlugin::destroy() {
    delete widget_;
    widget_ = nullptr;
}
