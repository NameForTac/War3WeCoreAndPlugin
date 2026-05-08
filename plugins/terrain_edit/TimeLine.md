# TerrainEditPlugin — TimeLine

## 2026-05-08

### 12:20 — 项目启动 & 文件框架创建
- 创建 `terrain_edit_types.h`：定义 `EditTool` 枚举（Raise/Lower/Smooth/Flatten/Paint）
- 创建 `terrain_edit_plugin.h/cpp`：插件生命周期管理
- 创建 `terrain_edit_window.h/cpp`：独立窗口（工具栏+状态栏+纹理选择器）
- 创建 `terrain_edit_widget.h/cpp`：3D OpenGL 渲染和编辑核心

### 12:25 — CMakeLists.txt 注册
- 在 `plugins/CMakeLists.txt` 中添加 `terrain_edit` 目标
- 链接 `w3x_core`, `Qt::Widgets`, `Qt::OpenGLWidgets`

### 12:27 — 首次编译
- **BUGFIX**: `EditTool` 在 `window.h` 和 `widget.h` 中重复定义 → 提取到 `terrain_edit_types.h` 统一引入
- **BUGFIX**: `uploadGrid()` 在 header 中缺失声明（声明为 `drawGrid()`）→ 修正为 `uploadGrid()`
- ✅ 编译通过，生成 `build/plugins/libterrain_edit.dll` (488KB)

### 12:30 — 运行单元测试验证回归
- ✅ 所有现有测试通过（test_buffer, test_w3i, test_w3u, test_w3e, test_doo, test_units_doo, test_wts, test_slk, test_archive, test_object_id_gen）

### 12:35 — 全量构建分发
- ✅ `build.ps1` 成功运行，`libterrain_edit.dll` 已部署到 `dist/GUI/plugins/`
- ✅ `libsample_tab.dll` 和 `libterrain_edit.dll` 均正确分发

### 13:00 — 排查 DLL 加载失败问题
- **ROOT CAUSE 1** — 插件部署路径不匹配
  - DLL 位于 `build/plugins/`，但 GUI 默认从 `build/gui/plugins/` 加载
  - `PluginRegistry::load_directory()` 在目录不存在时静默返回 0，无报错
  - **FIX**: `build.ps1` 构建后将 plugin DLL 拷贝到 `build/gui/plugins/`
- **ROOT CAUSE 2** — 菜单索引越界 (mainwindow.cpp)
  - `menuBar()->actions().at(3)` 指向的是 Help（索引3），而非 Plugins（索引2）
  - 插件菜单项被添加到 Help 菜单而非 Plugins 菜单
  - **FIX**: 改为 `.at(2)`，并添加注释说明菜单顺序
- ✅ 修复后验证：从 `build/gui/` 启动 GUI，stderr 显示 `TerrainEditWindow` 已创建
- ✅ 单元测试 10/10 通过

### 13:05 — UI/UX 优化第一批（TODO #1-#6）
- **工具图标** — QPainter 程序化图标（raise/lower/smooth/flatten/paint/texture toggle）
- **快捷键** — 1-5 选工具，[ / ] 调笔刷大小，Shift+[ / Shift+] 调强度
- **窗口持久化** — QSettings 保存/恢复窗口位置、工具、笔刷参数
- **地形跟随笔刷** — 48 点采样高度的 3D 笔刷预览圆圈
- **网格 LOD** — 根据相机距离动态调整网格疏密（step 1/2/4/8）
- **纹理颜色切换** — 工具栏增加 Toggle 按钮切换高度/纹理着色
- **BUGFIX**: wheelEvent 重复定义 → 删除多余的 static 版本，保留 namespace 版本
- **BUGFIX**: 缺失 `#include <QPainterPath>` → 补全
- ✅ 编译通过，10/10 测试通过

### 13:10 — 插件级别汉化（i18n）
- 创建 `translations/terrain_edit_zh_CN.ts` — 22 条 UI 字符串中文翻译
- CMake 集成 `lrelease` 自动编译 .ts → .qm
- `build.ps1` 插件 .qm 跟随 DLL 部署到 `build/gui/plugins/` 和 `dist/GUI/plugins/`
- `init()` 时自动加载系统语言对应的 .qm（fallback 到 zh_CN）
- `destroy()` 时 `removeTranslator` 清理，无 dangling pointer
- QTranslator 存储在成员变量 `translator_` 中，随插件生命周期管理
- ✅ 编译通过，22/22 translations finished and 0 unfinished

### 13:15 — Camera 修复 + 地形背面不可见
- **BUGFIX**: `tr()` 在 translator 安装前调用 → 翻译不生效 → 调整初始化顺序，translator 在 `window_` 创建前加载
- **BUGFIX**: `sinP = qSin(pitch)` 在 pitch=-35° 时为负 → `eye.y` 为负 → 相机在地底 → 射线拾取从地底发射 → 编辑操作全部失效 → 改为 `qAbs(sinP)`，相机始终在地形上方
- **BUGFIX**: 右键平移 dx/dy 符号取反 → 拖拽方向与预期相反 → `-= right` 改为 `+= right`，`+= forward` 改为 `-= forward`
- **ENHANCE**: 启用 `glEnable(GL_CULL_FACE)` + `glCullFace(GL_BACK)` → 地形背面不渲染
- **ENHANCE**: `cam_pitch_` 上限限制为 -15° → 防止相机降到地面高度，避免地形侧面透视
- ✅ 编译通过，10/10 测试通过
