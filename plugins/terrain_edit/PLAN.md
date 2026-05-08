# TerrainEditPlugin — 3D 地形编辑插件

外部 DLL 插件，在独立窗口中提供 w3e 地形的 3D 可视化与编辑功能。
完全基于 `IEditorPlugin` 接口和 `MapBuilder` API，不修改现有代码。

## 文件结构

```
plugins/terrain_edit/
├── PLAN.md                       # 本文件 — 实施计划与设计说明
├── terrain_edit_plugin.h/cpp     # IEditorPlugin 实现
├── terrain_edit_window.h/cpp     # 独立 QMainWindow（工具栏+3D视口+状态栏）
└── terrain_edit_widget.h/cpp     # QOpenGLWidget：3D渲染+编辑交互
```

## 插件类：TerrainEditPlugin

- 继承 `IEditorPlugin`
- 能力：`ProvidesMenu | NeedsSavable`
- `init()`: 创建 TerrainEditWindow 但不显示
- `activate()`: 通知窗口地图已加载，可读取 terrain
- `deactivate()`: 通知窗口地图已关闭
- `sync_to_builder()`: 将编辑后的 terrain 写回 `builder_->set_terrain()`
- `menu_actions()`: 返回一个 QAction "Terrain Editor..."，点击显示窗口

## 窗口类：TerrainEditWindow (QMainWindow)

- 独立于 MainWindow，有自己的任务栏入口
- **编辑工具栏**（左侧或顶部）：
  - 工具按钮组：Raise / Lower / Smooth / Flatten / Paint
  - 笔刷大小滑块 (1-20)
  - 笔刷强度滑块 (0.1-5.0)
- **中央**：TerrainEditWidget 作为 centralWidget
- **状态栏**：显示当前高度、笔刷信息、坐标
- 连接信号以跟踪脏状态

## 3D 控件类：TerrainEditWidget (QOpenGLWidget)

### 渲染

- 高度着色网格（同现有 TerrainWidget 方案）
- 网格线叠加 (GL_LINES)
- 笔刷预览半透明圆环
- 纹理颜色区分（按 ground_texture 索引上色）
- 可选瓦片边界线

### 相机控制

| 操作 | 行为 |
|------|------|
| 中键 / Alt+左键 | 旋转 (yaw/pitch) |
| 右键 | 平移 (pan) |
| 滚轮 | 缩放 |

完全沿袭现有 TerrainWidget 的相机实现。

### 编辑交互

| 工具 | 行为 |
|------|------|
| Raise | 鼠标拖拽时，笔刷半径内顶点高度增加 |
| Lower | 鼠标拖拽时，笔刷半径内顶点高度降低 |
| Smooth | 对笔刷范围内顶点做拉普拉斯均值滤波 |
| Flatten | 将笔刷范围内高度统一为点击点高度 |
| Paint | 修改笔刷范围内顶点的 ground_texture |

笔刷影响力使用线性 falloff：`weight = 1.0 - distance/radius`。

### 鼠标拾取 (Ray-Terrain Intersection)

1. 屏幕坐标 → 世界空间射线（逆 projection × view 矩阵）
2. 射线与 `y = height_at(col,row)` 平面求交 → 大致 tile 位置
3. 精确到目标顶点索引 `(col, row)`

## 数据流

```
[MainWindow 打开地图]
  → PluginRegistry::activate_all()
    → TerrainEditPlugin::activate()
      → TerrainEditorWindow::onMapLoaded()
        → builder_->read_terrain()
        → widget_->loadTerrain(t)

[用户编辑]
  → TerrainEditWidget 修改 Terrain 数据
  → 发出 terrainEdited() 信号
  → TerrainEditorWindow 设置脏标志

[MainWindow 保存]
  → PluginRegistry::sync_all()
    → TerrainEditPlugin::sync_to_builder()
      → builder_->set_terrain(widget_->getTerrain())

[关闭地图]
  → PluginRegistry::deactivate_all()
    → TerrainEditPlugin::deactivate()
      → TerrainEditorWindow::onMapClosed()
```

## 与内置 TerrainEditorPlugin 的对比

| 方面 | 内置 (gui/terraineditor) | 本插件 (DLL) |
|------|--------------------------|---------------|
| 集成 | `ProvidesTab`，嵌入标签页 | `ProvidesMenu`，独立 QMainWindow |
| 编辑 | 只读查看 | Raise/Lower/Smooth/Flatten/Paint |
| 保存 | 无 | `NeedsSavable` + `sync_to_builder()` |
| 窗口 | MainWindow 标签页 | 独立浮动窗口，可最小化/置顶 |

## 注册到构建

在 `plugins/CMakeLists.txt` 中添加：

```cmake
add_library(terrain_edit SHARED
    terrain_edit/terrain_edit_plugin.cpp
    terrain_edit/terrain_edit_window.cpp
    terrain_edit/terrain_edit_widget.cpp
)

target_include_directories(terrain_edit PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(terrain_edit PRIVATE w3x_core Qt::Widgets Qt::OpenGLWidgets)

set_target_properties(terrain_edit PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
```

## 实施步骤

1. ✅ 创建插件框架文件（3 个头文件 + 3 个 cpp） — 2026-05-08 12:20
2. ✅ 实现 `TerrainEditPlugin`：生命周期 + 菜单动作 — 2026-05-08 12:20
3. ✅ 实现 `TerrainEditWindow`：工具栏 + 布局 + 状态栏 — 2026-05-08 12:20
4. ✅ 实现 `TerrainEditWidget` 渲染（继承现有 3D 代码） — 2026-05-08 12:20
5. ✅ 实现相机控制 — 2026-05-08 12:20（沿袭现有 TerrainWidget 实现）
6. ✅ 实现鼠标拾取（ray-heightfield stepping） — 2026-05-08 12:20
7. ✅ 实现编辑操作：Raise/Lower/Smooth/Flatten/Paint — 2026-05-08 12:20
8. ✅ 编辑时实时更新 OpenGL（mesh_dirty_ 标志 + 每帧增量上传） — 2026-05-08 12:20
9. ✅ 注册到 plugins/CMakeLists.txt — 2026-05-08 12:25
10. ✅ 实现 DLL 工厂函数 `w3x_plugin_create()` — 2026-05-08 12:20

### 构建验证 (2026-05-08 12:27)
- 首次编译修复 2 个 BUG：`EditTool` 重复定义、`uploadGrid()` 声明缺失
- 编译通过，生成 `libterrain_edit.dll` (488KB)
- 单元测试 10/10 通过（无回归）
- `build.ps1` 全量构建通过，插件部署至 `dist/GUI/plugins/`

### DLL 加载问题排查 (2026-05-08 13:00)
- **BUGFIX**: `build.ps1` 缺少开发时插件拷贝步骤 → DLL 在 `build/plugins/`，GUI 在 `build/gui/plugins/` 查找
- **BUGFIX**: `mainwindow.cpp` 菜单索引写死 `at(3)` 应为 `at(2)` → 插件菜单项被添加到 Help 而非 Plugins
- 修复后验证通过

详情见 [TimeLine.md](TimeLine.md)

## 国际化/本地化

插件拥有独立的翻译系统，不依赖主程序的翻译文件：

1. **翻译文件位置** — `translations/terrain_edit_zh_CN.ts`，存放在插件目录内而非 core
2. **编译** — CMake 自动调用 `lrelease` 将 .ts 编译为 .qm，输出到 DLL 同目录
3. **加载** — `TerrainEditPlugin::init()` 中创建 `QTranslator`，按系统 locale 加载对应 .qm（fallback 到 zh_CN）
4. **清理** — `TerrainEditPlugin::destroy()` 中调用 `removeTranslator()`，无 dangling pointer
5. **新增字符串** — 所有 UI 字符串必须使用 `tr()` 包裹，然后更新 .ts 文件

### 文件结构

```
plugins/terrain_edit/
├── translations/
│   └── terrain_edit_zh_CN.ts       # 中文翻译源
├── terrain_edit_zh_CN.qm           # 编译后（build output）
└── ...
```
