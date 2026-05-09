#include "terrain_edit_widget.h"
#include "../../src/core/map_builder.h"
#include <QPainter>
#include <QtMath>
#include <QApplication>
#include <algorithm>
#include <cmath>

// ============================================================
// Construction / Destruction
// ============================================================
TerrainEditWidget::TerrainEditWidget(QWidget* parent)
    : TerrainRendererBase(parent)
{
}

TerrainEditWidget::~TerrainEditWidget() {
    // Base destructor handles programs, textures cleanup (via destroyGPUBuffers + freeTextures)
}

// ============================================================
// Load terrain
// ============================================================
void TerrainEditWidget::loadTerrain(Terrain* terrain) {
    if (isValid())
        makeCurrent();
    destroyGPUBuffers();
    freeTextures();

    terrain_ = terrain;
    has_terrain_ = (terrain != nullptr);

    if (has_terrain_) {
        // Buffers will be created lazily in paintGL() when the GL context is ready
        geometry_dirty_ = true;
        tex_dirty_ = true;
        float cx = terrain_->center_offset_x + (terrain_->tile_width - 1) * kTileSize * 0.5f;
        float cz = terrain_->center_offset_y + (terrain_->tile_height - 1) * kTileSize * 0.5f;
        cam_center_ = QPointF(cx, cz);
        cam_yaw_ = -45.0f;
        cam_pitch_ = -35.0f;

        float max_dim = static_cast<float>(
            qMax(terrain_->tile_width, terrain_->tile_height)) * kTileSize;
        float pitch_rad = qDegreesToRadians(qAbs(cam_pitch_));
        float cosP = qCos(pitch_rad);
        float needed_dist = (max_dim * 0.5f) / (cosP * 0.375f);
        cam_distance_ = qBound(100.0f, needed_dist, 100000.0f);
    }

    hover_col_ = -1;
    hover_row_ = -1;
    update();
}

// ============================================================
// Color helpers
// ============================================================
QColor TerrainEditWidget::heightColor(float h) const {
    if (!terrain_ || terrain_->tilepoints.empty())
        return QColor::fromRgbF(0.4f, 0.6f, 0.3f);

    auto [min_it, max_it] = std::minmax_element(
        terrain_->tilepoints.begin(), terrain_->tilepoints.end(),
        [](const TilePoint& a, const TilePoint& b) { return a.height < b.height; });
    float min_h = min_it->height;
    float max_h = max_it->height;
    if (max_h - min_h < 1.0f) { min_h -= 50.0f; max_h += 50.0f; }

    float t = qBound(0.0f, (h - min_h) / (max_h - min_h), 1.0f);
    if (t < 0.5f) {
        float s = t * 2.0f;
        return QColor::fromRgbF(0.2f + s * 0.4f, 0.4f + s * 0.2f, 0.1f + s * 0.1f);
    } else {
        float s = (t - 0.5f) * 2.0f;
        return QColor::fromRgbF(0.6f + s * 0.3f, 0.6f + s * 0.3f, 0.2f + s * 0.6f);
    }
}

QColor TerrainEditWidget::textureColor(int tex_idx) const {
    static const QColor colors[] = {
        QColor(120,140,100), QColor(100,180,80),  QColor(180,160,100),
        QColor(160,120,80),  QColor(200,180,130), QColor(140,200,120),
        QColor(130,110,80),  QColor(210,200,150), QColor(100,100,120),
        QColor(160,150,140), QColor(80,100,60),   QColor(200,180,110),
        QColor(140,120,100), QColor(190,190,210), QColor(100,140,180),
        QColor(70,70,70),
    };
    return (tex_idx >= 0 && tex_idx < 16) ? colors[tex_idx] : colors[0];
}

QColor TerrainEditWidget::tileColor(const QString& tileId, int fallback_idx) const {
    if (tileId.length() >= 4) {
        QString type = tileId.mid(1, 3);
        if (type == "grs" || type == "pla") return {70, 150, 50};
        if (type == "grh") return {60, 130, 40};
        if (type == "drt") return {160, 130, 85};
        if (type == "dro" || type == "roc") return {145, 115, 70};
        if (type == "rok") return {120, 115, 105};
        if (type == "sor" || type == "sqd") return {155, 145, 130};
        if (type == "snw" || type == "hwa") return {200, 210, 220};
        if (type == "dsr") return {205, 175, 110};
        if (type == "blp") return {55, 50, 45};
        if (type == "bbb") return {130, 100, 70};
        if (type == "clf") return {105, 95, 72};
        if (type == "wtf" || type == "gtf") return {145, 140, 130};
        if (type == "hdg") return {50, 95, 35};
        if (type == "dos") return {55, 100, 35};
    }
    return textureColor(fallback_idx);
}

// ============================================================
// OpenGL initialization
// ============================================================
void TerrainEditWidget::initializeGL() {
    TerrainRendererBase::initializeGL();
    initGridShaders();
}

// ============================================================
// Rendering hooks
// ============================================================
void TerrainEditWidget::onBeforePaint() {
    // Override lighting / polygon mode based on render mode
    if (render_mode_ == RenderMode::Wireframe) {
        glUniform1f(u_lighting_, 0.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glUniform1f(u_lighting_, (render_mode_ == RenderMode::Lit) ? 1.0f : 0.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Texture toggle
    if (!show_texture_)
        glUniform1f(u_use_tex_, 0.0f);
}

void TerrainEditWidget::onAfterPaint() {
    // Disable culling for overlays
    glDisable(GL_CULL_FACE);

    // --- Draw brush preview ---
    if (hover_col_ >= 0 && hover_row_ >= 0)
        drawBrushPreview();

    // --- Draw center marker ---
    drawCenterMarker();

    glEnable(GL_CULL_FACE);
    glUseProgram(0);

    // --- Draw help overlay (QPainter, no GL state needed) ---
    drawHelpOverlay();
}

// ============================================================
// Grid with LOD
// ============================================================
// ============================================================
// Brush preview — follows terrain surface
// ============================================================
void TerrainEditWidget::drawBrushPreview() {
    if (!terrain_ || !grid_program_) return;

    auto draw_verts = [&](const float* verts, int count, int stride) {
        glBindVertexArray(vao_);
        GLuint tmp_vbo;
        glGenBuffers(1, &tmp_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, tmp_vbo);
        glBufferData(GL_ARRAY_BUFFER, count * stride, verts, GL_STREAM_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINE_LOOP, 0, count);
        glDisableVertexAttribArray(0);
        glDeleteBuffers(1, &tmp_vbo);
        glBindVertexArray(0);
    };

    float cx = terrain_->center_offset_x + hover_col_ * kTileSize;
    float cz = terrain_->center_offset_y + hover_row_ * kTileSize;
    float radius_world = brush_size_ * kTileSize;

    int tw = terrain_->tile_width;

    glUseProgram(grid_program_);
    glUniformMatrix4fv(u_grid_mvp_, 1, GL_FALSE, last_mvp_.constData());
    glUniform4f(glGetUniformLocation(grid_program_, "u_color"), 1.0f, 1.0f, 0.0f, 0.7f);

    if (brush_shape_ == BrushShape::Square) {
        float hw = radius_world;
        float corners[4][2] = {
            {cx - hw, cz - hw}, {cx + hw, cz - hw},
            {cx + hw, cz + hw}, {cx - hw, cz + hw}
        };
        float verts[5][3];
        for (int i = 0; i < 5; ++i) {
            int ci = i % 4;
            float wx = corners[ci][0];
            float wz = corners[ci][1];
            float fc = (wx - terrain_->center_offset_x) / kTileSize;
            float fr = (wz - terrain_->center_offset_y) / kTileSize;
            int cci = qBound(0, qRound(fc), tw - 1);
            int rri = qBound(0, qRound(fr), terrain_->tile_height - 1);
            float wy = (terrain_->tilepoints[rri * tw + cci].height - 8192.0f) * 0.25f + 5.0f;
            verts[i][0] = wx; verts[i][1] = wy; verts[i][2] = wz;
        }
        draw_verts(&verts[0][0], 5, (int)sizeof(verts[0]));
    } else {
        const int segs = 48;
        float verts[(segs + 1) * 3];
        for (int i = 0; i <= segs; ++i) {
            float a = 2.0f * 3.14159265f * i / segs;
            float wx = cx + radius_world * cosf(a);
            float wz = cz + radius_world * sinf(a);
            float fc = (wx - terrain_->center_offset_x) / kTileSize;
            float fr = (wz - terrain_->center_offset_y) / kTileSize;
            int ci = qBound(0, qRound(fc), tw - 1);
            int ri = qBound(0, qRound(fr), terrain_->tile_height - 1);
            float wy = (terrain_->tilepoints[ri * tw + ci].height - 8192.0f) * 0.25f + 5.0f;
            verts[i * 3]     = wx;
            verts[i * 3 + 1] = wy;
            verts[i * 3 + 2] = wz;
        }
        draw_verts(verts, segs + 1, (int)(3 * sizeof(float)));
    }
}

void TerrainEditWidget::drawCenterMarker() {
    float cx = cam_center_.x(), cz = cam_center_.y();
    glUseProgram(grid_program_);
    glUniformMatrix4fv(u_grid_mvp_, 1, GL_FALSE, last_mvp_.constData());
    auto draw_line = [&](float x1, float y1, float z1,
                         float x2, float y2, float z2,
                         float r, float g, float b, float a)
    {
        glUniform4f(glGetUniformLocation(grid_program_, "u_color"), r, g, b, a);
        float verts[] = {x1, y1, z1, x2, y2, z2};
        GLuint vao2, vbo2;
        glGenVertexArrays(1, &vao2);
        glGenBuffers(1, &vbo2);
        glBindVertexArray(vao2);
        glBindBuffer(GL_ARRAY_BUFFER, vbo2);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, 0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINES, 0, 2);
        glDeleteVertexArrays(1, &vao2);
        glDeleteBuffers(1, &vbo2);
        glBindVertexArray(0);
    };
    draw_line(cx - 30, 0, cz, cx + 30, 0, cz, 1, 0, 0, 1);
    draw_line(cx, 0, cz - 30, cx, 0, cz + 30, 0, 0, 1, 1);
}

// ============================================================
// Help overlay
// ============================================================
void TerrainEditWidget::drawHelpOverlay() {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int ow = 260, oh = 250;
    int mx = width() - ow - 10, my = 10;

    p.fillRect(mx, my, ow, oh, QColor(0, 0, 0, 160));
    p.setPen(QColor(255, 255, 200));
    QFont f = p.font();
    f.setPointSize(10); f.setBold(true); p.setFont(f);
    p.drawText(mx + 8, my + 16, tr("Controls"));

    f.setPointSize(8); f.setBold(false); p.setFont(f);
    p.setPen(QColor(220, 220, 220));
    int y = my + 30;
    auto draw_help_line = [&](const char* key, const QString& desc) {
        p.drawText(mx + 8, y, QString::fromUtf8(key) + QStringLiteral("  ") + desc);
        y += 18;
    };

    draw_help_line("LMB",           tr("Edit terrain"));
    draw_help_line("MMB / Alt+LMB", tr("Orbit"));
    draw_help_line("RMB",           tr("Pan"));
    draw_help_line("Scroll",        tr("Zoom"));
    draw_help_line("1-5",           tr("Select tool"));
    draw_help_line("[ / ]",         tr("Brush size"));
    draw_help_line("Shift+[ / ]",   tr("Brush strength"));
    draw_help_line("B",             tr("Brush shape"));
    draw_help_line("Ctrl+Z",        tr("Undo"));
    draw_help_line("Ctrl+Y",        tr("Redo"));
    draw_help_line("F",             tr("Focus tile"));

    f.setPointSize(7); f.setBold(true); p.setFont(f);
    QString modeText;
    switch (render_mode_) {
    case RenderMode::Wireframe: modeText = tr("Wireframe"); break;
    case RenderMode::Unlit:     modeText = tr("Unlit");     break;
    case RenderMode::Lit:       modeText = tr("Lit");       break;
    }
    p.setPen(QColor(180, 200, 255));
    p.drawText(mx + 8, my + oh - 10, tr("Render mode") + QStringLiteral(": ") + modeText);

    // Debug overlay — show render state
    f.setPointSize(8); f.setBold(false); p.setFont(f);
    int dy = 10;
    p.fillRect(10, dy - 2, 380, 90, QColor(0, 0, 0, 160));
    p.setPen(QColor(255, 100, 100));
    p.drawText(14, dy += 14, QStringLiteral("program=%1 vao=%2 texArray=%3 texCnt=%4 showTex=%5")
        .arg(program_).arg(vao_).arg(tex_array_).arg(tex_count_).arg(show_texture_));
    p.drawText(14, dy += 14, QStringLiteral("terrainVao=%1 terrainVbo=%2 verts=%3 geoDirty=%4")
        .arg(terrain_vao_).arg(terrain_vbo_).arg(terrain_vertex_count_).arg(geometry_dirty_));
    p.drawText(14, dy += 14, QStringLiteral("hasTer=%1 terrain=%2 w=%3 h=%4 uv_tex=%5")
        .arg(has_terrain_).arg((quintptr)terrain_, 0, 16)
        .arg(terrain_ ? terrain_->tile_width : 0)
        .arg(terrain_ ? terrain_->tile_height : 0)
        .arg(u_tex_array_));
    p.drawText(14, dy += 14, QStringLiteral("texMode=%1 light=%2 pitch=%3 yaw=%4 dist=%5")
        .arg(render_mode_ == RenderMode::Wireframe ? "WIRE" :
             render_mode_ == RenderMode::Unlit ? "UNLIT" : "LIT")
        .arg(u_lighting_).arg(cam_pitch_, 0, 'f', 1).arg(cam_yaw_, 0, 'f', 1).arg(cam_distance_, 0, 'f', 0));
    p.drawText(14, dy += 14, QStringLiteral("showTex=%1 tex_dirty=%2 verts=%3")
        .arg(show_texture_).arg(tex_dirty_).arg(terrain_vertex_count_));

    p.end();
}

// ============================================================
// Mouse picking (ray cast against heightfield)
// ============================================================
bool TerrainEditWidget::pickTile(int screen_x, int screen_y, int& col, int& row) {
    if (!has_terrain_ || !terrain_) return false;
    int w = width(), h = height();
    if (h == 0 || w == 0) return false;

    float ndc_x = (2.0f * screen_x / w) - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y / h);
    float aspect = (float)w / (float)h;

    QMatrix4x4 mvp = computeMVPMatrix(aspect);

    bool invertible = false;
    QMatrix4x4 inv_mvp = mvp.inverted(&invertible);
    if (!invertible) return false;

    QVector4D near_ws = inv_mvp * QVector4D(ndc_x, ndc_y, -1.0f, 1.0f);
    QVector4D far_ws  = inv_mvp * QVector4D(ndc_x, ndc_y, 1.0f, 1.0f);
    if (near_ws.w() == 0 || far_ws.w() == 0) return false;
    near_ws /= near_ws.w();
    far_ws  /= far_ws.w();

    QVector3D ray_origin(near_ws.x(), near_ws.y(), near_ws.z());
    QVector3D ray_dir = QVector3D(far_ws.x(), far_ws.y(), far_ws.z()) - ray_origin;
    ray_dir.normalize();
    if (qAbs(ray_dir.y()) < 0.0001f) return false;

    float t_step = kTileSize * 0.5f;
    float max_dist = cam_distance_ * 3.0f;
    for (float t = 0.0f; t < max_dist; t += t_step) {
        QVector3D pos = ray_origin + ray_dir * t;
        float fc = (pos.x() - terrain_->center_offset_x) / kTileSize;
        float fr = (pos.z() - terrain_->center_offset_y) / kTileSize;
        int ci = qBound(0, qRound(fc), terrain_->tile_width - 1);
        int ri = qBound(0, qRound(fr), terrain_->tile_height - 1);
        float raw_h = terrain_->tilepoints[ri * terrain_->tile_width + ci].height;
        float world_y = (raw_h - 8192.0f) * 0.25f;
        if (pos.y() <= world_y) { col = ci; row = ri; return true; }
    }
    return false;
}

// ============================================================
// Editing operations
// ============================================================
void TerrainEditWidget::applyBrush(int center_col, int center_row) {
    if (!has_terrain_ || !terrain_) return;

    int radius = brush_size_;
    int min_c = std::max(0, center_col - radius);
    int max_c = std::min(terrain_->tile_width - 1, center_col + radius);
    int min_r = std::max(0, center_row - radius);
    int max_r = std::min(terrain_->tile_height - 1, center_row + radius);

    for (int r = min_r; r <= max_r; ++r) {
        for (int c = min_c; c <= max_c; ++c) {
            float dr = (float)(r - center_row);
            float dc = (float)(c - center_col);
            float dist = std::sqrt(dr * dr + dc * dc);
            if (brush_shape_ != BrushShape::Square && dist > (float)radius) continue;

            switch (current_tool_) {
            case EditTool::Raise:   raiseLower(c, r, dist, (float)radius, 1.0f); break;
            case EditTool::Lower:   raiseLower(c, r, dist, (float)radius, -1.0f); break;
            case EditTool::Smooth:  smoothOp(c, r, dist, (float)radius); break;
            case EditTool::Flatten: flattenOp(c, r, dist, (float)radius); break;
            case EditTool::Paint:   paintOp(c, r, dist, (float)radius); break;
            }
        }
    }

    geometry_dirty_ = true;
    update();
}

static float clampHeight(float h) {
    return qBound(-8192.0f, h, 8192.0f);
}

void TerrainEditWidget::raiseLower(int col, int row, float dist, float radius, float dir) {
    float falloff = 1.0f - (dist / radius);
    float delta = dir * brush_strength_ * 8.0f * falloff;
    int idx = row * terrain_->tile_width + col;
    terrain_->tilepoints[idx].height = clampHeight(terrain_->tilepoints[idx].height + delta);
}

void TerrainEditWidget::smoothOp(int col, int row, float dist, float radius) {
    int w = terrain_->tile_width;
    int idx = row * w + col;
    float sum = 0; int count = 0;
    if (col > 0)                         { sum += terrain_->tilepoints[idx - 1].height; count++; }
    if (col < w - 1)                     { sum += terrain_->tilepoints[idx + 1].height; count++; }
    if (row > 0)                         { sum += terrain_->tilepoints[idx - w].height; count++; }
    if (row < terrain_->tile_height - 1) { sum += terrain_->tilepoints[idx + w].height; count++; }
    if (count == 0) return;
    float avg = sum / count;
    float falloff = 1.0f - (dist / radius);
    float delta = (avg - terrain_->tilepoints[idx].height) * brush_strength_ * falloff * 0.3f;
    terrain_->tilepoints[idx].height = clampHeight(terrain_->tilepoints[idx].height + delta);
}

void TerrainEditWidget::flattenOp(int col, int row, float dist, float radius) {
    int idx = row * terrain_->tile_width + col;
    float falloff = 1.0f - (dist / radius);
    float delta = (flatten_target_ - terrain_->tilepoints[idx].height) * brush_strength_ * falloff * 0.3f;
    terrain_->tilepoints[idx].height = clampHeight(terrain_->tilepoints[idx].height + delta);
}

void TerrainEditWidget::paintOp(int col, int row, float /*dist*/, float /*radius*/) {
    int idx = row * terrain_->tile_width + col;
    terrain_->tilepoints[idx].ground_texture = (uint8_t)paint_texture_;
}

// ============================================================
// Undo / Redo
// ============================================================
void TerrainEditWidget::pushUndo() {
    if (!terrain_) return;
    undo_stack_.push_back(*terrain_);
    if ((int)undo_stack_.size() > kMaxUndo_)
        undo_stack_.erase(undo_stack_.begin());
    redo_stack_.clear();
}

void TerrainEditWidget::undo() {
    if (!terrain_ || undo_stack_.empty()) return;
    redo_stack_.push_back(*terrain_);
    *terrain_ = undo_stack_.back();
    undo_stack_.pop_back();
    destroyGPUBuffers();
    createGPUBuffers();
    hover_col_ = -1; hover_row_ = -1;
    editing_ = false;
    emit terrainEdited();
    update();
}

void TerrainEditWidget::redo() {
    if (!terrain_ || redo_stack_.empty()) return;
    undo_stack_.push_back(*terrain_);
    *terrain_ = redo_stack_.back();
    redo_stack_.pop_back();
    destroyGPUBuffers();
    createGPUBuffers();
    hover_col_ = -1; hover_row_ = -1;
    editing_ = false;
    emit terrainEdited();
    update();
}

// ============================================================
// GPU resource cleanup (extended for grid VAO/VBO)
// ============================================================

void TerrainEditWidget::focusOnTile(int col, int row) {
    if (!terrain_) return;
    col = qBound(0, col, terrain_->tile_width - 1);
    row = qBound(0, row, terrain_->tile_height - 1);
    float wx = terrain_->center_offset_x + col * kTileSize;
    float wz = terrain_->center_offset_y + row * kTileSize;
    cam_center_ = QPointF(wx, wz);
    cam_distance_ = qMax(cam_distance_ * 0.5f, 100.0f);
    hover_col_ = col; hover_row_ = row;
    update();
}

// ============================================================
// Mouse events
// ============================================================
void TerrainEditWidget::mousePressEvent(QMouseEvent* event) {
    if (!has_terrain_) return;

    if (event->button() == Qt::LeftButton &&
        !(QApplication::keyboardModifiers() & Qt::AltModifier))
    {
        int col, row;
        if (pickTile((int)event->position().x(), (int)event->position().y(), col, row)) {
            pushUndo();
            editing_ = true;
            hover_col_ = col; hover_row_ = row;
            if (current_tool_ == EditTool::Flatten)
                flatten_target_ = terrain_->tilepoints[row * terrain_->tile_width + col].height;
            applyBrush(col, row);
            emit terrainEdited();
            update();
        }
        return;
    }

    baseMousePress(event);
}

void TerrainEditWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!has_terrain_ || !terrain_) return;

    if (editing_) {
        int col, row;
        if (pickTile((int)event->position().x(), (int)event->position().y(), col, row)) {
            bool changed = (col != hover_col_ || row != hover_row_);
            hover_col_ = col; hover_row_ = row;
            applyBrush(col, row);
            if (changed) emit terrainEdited();
            update();
        }
        return;
    }

    if (!dragging_) {
        int col, row;
        if (pickTile((int)event->position().x(), (int)event->position().y(), col, row)) {
            if (col != hover_col_ || row != hover_row_) { hover_col_ = col; hover_row_ = row; update(); }
            int idx = row * terrain_->tile_width + col;
            const auto& tp = terrain_->tilepoints[idx];
            QString texName;
            if (tp.ground_texture < (int)terrain_->ground_tiles.size())
                texName = QString::fromStdString(terrain_->ground_tiles[tp.ground_texture]);
            else
                texName = QString::number(tp.ground_texture);
            emit statusMessage(QStringLiteral("Tile (%1, %2)  H: %3  Tex: %4")
                .arg(col).arg(row).arg(tp.height, 0, 'f', 1).arg(texName));
        } else {
            if (hover_col_ >= 0 || hover_row_ >= 0) { hover_col_ = -1; hover_row_ = -1; update(); }
            emit statusMessage(QString());
        }
        return;
    }

    baseMouseMove(event);
}

void TerrainEditWidget::mouseReleaseEvent(QMouseEvent* /*event*/) {
    editing_ = false;
    baseMouseRelease(nullptr);
}

void TerrainEditWidget::wheelEvent(QWheelEvent* event) {
    if (!has_terrain_) return;
    baseWheelEvent(event);
    event->accept();
}

// ============================================================
// GPU resource cleanup (extended for grid VAO/VBO)
// ============================================================
void TerrainEditWidget::destroyGPUBuffers() {
    TerrainRendererBase::destroyGPUBuffers();
}
