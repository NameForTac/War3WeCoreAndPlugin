# w3x-packer 插件开发指南

## 概述

w3x-packer 支持运行时加载的 DLL 插件。插件通过实现 `IEditorPlugin` 纯虚接口来扩展编辑器功能，可以添加标签页、菜单项、工具栏按钮，以及在保存时同步数据到 MapBuilder。

### 架构

```
┌──────────────────────────────────────────────┐
│  MainWindow (核心：仅打开/保存/列文件)         │
├──────────────────────────────────────────────┤
│  PluginRegistry (单例，管理插件全生命周期)     │
│  ┌──────────────────┐  ┌──────────────────┐  │
│  │  内置插件 (编译入) │  │  外部 DLL 插件   │  │
│  │  MapInfoPlugin    │  │  sample_tab.dll   │  │
│  │  FileBrowserPlugin│  │  你的插件 DLL    │  │
│  │  ObjectEditor...  │  │  w3x_plugin_... │  │
│  │  TerrainEditor... │  │                  │  │
│  │  PlacementEditor  │  │                  │  │
│  └──────────────────┘  └──────────────────┘  │
├──────────────────────────────────────────────┤
│  PluginContext                                │
│  (MapBuilder* + MetaDataDB* + 父窗口)         │
├──────────────────────────────────────────────┤
│  MapBuilder (读取/修改地图数据)                │
│  w3i / w3u/w3e / doo / wts / ...             │
└──────────────────────────────────────────────┘
```

## 快速开始

### 最小插件

**plugindemo.h:**

```cpp
#pragma once

#include "plugin.h"
#include <QWidget>
#include <QLabel>

class DemoPlugin : public IEditorPlugin {
public:
    // IEditorPlugin
    QString name() const override { return "Demo"; }
    QString version() const override { return "1.0.0"; }
    PluginCapability capabilities() const override {
        return PluginCapability::ProvidesTab;
    }

    bool init(PluginContext& ctx) override;
    void activate() override;
    void deactivate() override;
    void destroy() override;

    QWidget* tab_widget() override { return widget_; }
    QString tab_title() override { return "Demo"; }

private:
    QWidget* widget_ = nullptr;
    QLabel* label_ = nullptr;
};
```

**plugindemo.cpp:**

```cpp
#include "plugindemo.h"
#include <QVBoxLayout>

bool DemoPlugin::init(PluginContext& ctx) {
    widget_ = new QWidget(ctx.parent_widget);
    auto* layout = new QVBoxLayout(widget_);
    label_ = new QLabel("Hello from Demo plugin!");
    layout->addWidget(label_);
    return true;
}

void DemoPlugin::activate() {
    if (label_)
        label_->setText("Map loaded! Plugin active.");
}

void DemoPlugin::deactivate() {
    if (label_)
        label_->setText("Map unloaded. Plugin inactive.");
}

void DemoPlugin::destroy() {
    delete widget_;
    widget_ = nullptr;
    label_ = nullptr;
}

extern "C" __declspec(dllexport) IEditorPlugin* w3x_plugin_create() {
    return new DemoPlugin();
}
```

**CMakeLists.txt:**

```cmake
add_library(plugindemo SHARED
    plugindemo.cpp
)

target_include_directories(plugindemo PRIVATE
    ${CMAKE_SOURCE_DIR}/src          # for plugin.h / MapBuilder types
)

target_link_libraries(plugindemo PRIVATE
    w3x_core                         # link against the core library
    Qt::Widgets                      # Qt widgets
)

# Output to build/plugins/ for dist packaging
set_target_properties(plugindemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
```

### 构建和测试

```bash
# 完整构建（含插件）
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DWITH_GUI=ON
cmake --build build

# 将插件 DLL 放入插件目录后启动 GUI
# 默认插件目录: {exe_dir}/plugins/
# 可通过 设置 → 插件目录 修改
```

## IEditorPlugin 接口参考

```cpp
class IEditorPlugin {
public:
    virtual ~IEditorPlugin() = default;

    // ── 元信息 ──
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual PluginCapability capabilities() const = 0;

    // ── 生命周期 ──
    virtual bool init(PluginContext& ctx) = 0;
    virtual void activate() = 0;
    virtual void deactivate() = 0;
    virtual void destroy() = 0;

    // ── 标签页 ── (Requires ProvidesTab)
    virtual QWidget* tab_widget() { return nullptr; }
    virtual QString tab_title() { return QString(); }

    // ── 保存 ── (Requires NeedsSavable)
    virtual void sync_to_builder() {}

    // ── 菜单/工具栏 ── (Requires ProvidesMenu / ProvidesTool)
    virtual QList<QAction*> menu_actions() { return {}; }
    virtual QList<QAction*> toolbar_actions() { return {}; }
};
```

### 生命周期

```
注册/加载 →  init(PluginContext) →  创建 UI
                ↓
打开地图  →  activate()          →  读取数据、填充 UI
                ↓
编辑操作   →  (用户交互)
                ↓
保存地图  →  sync_to_builder()   →  将修改写回 MapBuilder (仅 NeedsSavable)
                ↓
关闭地图  →  deactivate()        →  清空 UI
                ↓
卸载插件  →  destroy()           →  释放资源
                ↓
 (DLL)   →  FreeLibrary
```

### 各阶段职责

| 阶段 | 做的事 | 不要做 |
|------|--------|--------|
| `init` | 创建 QWidget、布局、子控件。保存 PluginContext 供后续使用。 | 不要读取地图数据（地图尚未打开）。 |
| `activate` | 通过 `ctx.builder` 读取地图数据，填充 UI。启用标签页/按钮。 | 不要创建/销毁 QWidget（已在 init 中创建）。 |
| `deactivate` | 清空 UI 状态，释放数据缓存。 | 不要销毁 QWidget（destroy 中再做）。 |
| `sync_to_builder` | 将 UI 修改写回 MapBuilder 的缓存（如 `set_w3i`、`set_object`）。 | 不要执行文件 I/O、不要弹出对话框。 |
| `destroy` | `delete widget_`，置零所有指针。 | 不要访问 builder（可能已失效）。 |

## PluginCapability 位域

```cpp
enum class PluginCapability {
    None        = 0,
    ProvidesTab   = 1 << 0,  // 提供 QTabWidget 标签页
    ProvidesMenu  = 1 << 1,  // 添加菜单项到 Plugins 菜单
    ProvidesTool  = 1 << 2,  // 添加按钮到主工具栏
    NeedsSavable  = 1 << 3,  // 保存前需要同步数据
};
```

组合示例：

```cpp
// 标签页 + 可保存
PluginCapability capabilities() const override {
    return PluginCapability::ProvidesTab | PluginCapability::NeedsSavable;
}

// 纯工具栏按钮（无标签页）
PluginCapability capabilities() const override {
    return PluginCapability::ProvidesTool;
}
```

## 内置插件 vs DLL 插件

插件可以以两种形式存在：

| | 内置插件 (Compiled-in) | DLL 插件 |
|------|----------------------|----------|
| **注册方式** | `PluginRegistry::register_plugin()` | DLL 导出 `w3x_plugin_create()` |
| **构建** | 编译入 w3x-gui.exe | 单独编译为 .dll |
| **链接** | 可直接使用 gui/ 内部类 | 仅能链接 w3x_core + Qt |
| **部署** | 随主程序发布 | 放入 plugins/ 目录 |
| **适用场景** | 核心编辑器功能 | 第三方扩展 |

内置插件使用 `register_plugin()` 注册实例，DLL 插件通过扫描目录自动发现。

```cpp
// MainWindow 启动时注册内置插件
auto& reg = PluginRegistry::instance();
reg.register_plugin(std::make_unique<MapInfoPlugin>());
reg.register_plugin(std::make_unique<ObjectEditorPlugin>(&meta_db_));

// 再加载外部 DLL 插件
reg.load_directory(plugin_dir_);
reg.init_all(ctx);
```

## PluginContext

```cpp
struct PluginContext {
    MapBuilder* builder = nullptr;    // 地图数据编排器，读写地图数据
    MetaDataDB*  meta_db = nullptr;   // WC3 元数据 (SLK 字段 + 对象名称 + WESTRING 解析)
    QWidget*    parent_widget = nullptr; // 主窗口，可作为 QWidget 构造的 parent
};
```

在 `init()` 中保存 Context，后续在 `activate()` / `sync_to_builder()` 中使用：

```cpp
bool MyPlugin::init(PluginContext& ctx) {
    ctx_ = ctx;  // 保存副本
    // ...
}

void MyPlugin::activate() {
    // 读取地图信息
    auto info = ctx_.builder->read_w3i();
    // 读取地形
    auto terrain = ctx_.builder->read_terrain();
    // 读取单位摆放
    auto units = ctx_.builder->read_placed_units();
    // 解析对象名称（需要 WC3 数据目录配置）
    auto* name = ctx_.meta_db->find_object_name("hfoo"); // → "Footman"
}
```

### MetaDataDB 说明

`MetaDataDB` 通过 `PluginContext.meta_db` 传递给所有插件。它提供：

| 方法 | 说明 |
|------|------|
| `find_field(type, id)` | 查找字段元数据（类型/范围/默认值） |
| `get_fields(type)` | 获取某类型全部字段列表 |
| `find_object_name(id)` | 查询对象显示名称（来自 *strings.txt） |
| `resolve_object_name(id)` | 同 find，失败时返回原始 ID |
| `get_known_objects(type)` | 获取全部已知对象列表 |

仅在用户配置了 WC3 数据目录（设置 → WC3 数据目录）时可用。未配置时 `meta_db` 仍为非空指针但 `is_loaded()` 返回 false。

## MapBuilder API（插件常用操作）

### 读取方法

| 方法 | 返回类型 | 说明 |
|------|----------|------|
| `read_w3i()` | `MapInfo` | 地图信息（名称/作者/玩家/队伍等）|
| `read_object(name)` | `ObjectFile` | 对象文件（w3u/w3t/w3a/...）|
| `read_terrain()` | `Terrain` | 地形数据（w3e 瓦片网格） |
| `read_placed_units()` | `PlacedUnits` | 单位/物品摆放 |
| `read_placed_doodads()` | `PlacedDoodads` | 装饰物摆放 |
| `read_wts()` | `WTS` | 触发器字符串表 |
| `read_slk(name)` | `SLKTable` | 任意 SLK 文件 |
| `read_raw(name)` | `vector<uint8_t>` | 任意文件的原始字节 |
| `list_files()` | `vector<string>` | 列出地图内所有文件 |

### 修改方法

| 方法 | 说明 |
|------|------|
| `set_w3i(info)` | 更新地图信息 |
| `set_terrain(terrain)` | 更新地形数据 |
| `set_placed_units(units)` | 更新单位摆放 |
| `set_placed_doodads(doodads)` | 更新装饰物摆放 |
| `set_object(name, obj)` | 更新对象文件 |
| `set_wts(table)` | 更新字符串表 |
| `set_file(name, data)` | 覆盖/添加任意文件 |
| `remove_file(name)` | 从地图中移除文件 |

### 在 sync_to_builder 中的典型用法

```cpp
void MyPlugin::sync_to_builder() {
    // 将 UI 中的修改提交到 builder 缓存
    ctx_.builder->set_terrain(edited_terrain_);
    ctx_.builder->set_w3i(edited_map_info_);
}
```

## 格式详解

### 对象文件 (w3u / w3t / w3a / ...)

7 种对象编辑器文件共享同一格式（定义在 `w3u.h/cpp`）：

| 文件 | 对象类型 | 说明 |
|------|----------|------|
| `war3map.w3u` | Units | 单位 |
| `war3map.w3t` | Items | 物品 |
| `war3map.w3b` | Destructables | 可破坏物 |
| `war3map.w3d` | Doodads | 装饰物 |
| `war3map.w3a` | Abilities | 技能 |
| `war3map.w3h` | Buffs | 魔法效果 |
| `war3map.w3q` | Upgrades | 升级 |

核心数据结构：

```cpp
struct Modification {
    std::string mod_id;          // 4 字符修改 ID（如 "uhpm"）
    ValueType value_type;        // 0=Int, 1=Real, 2=Unreal, 3=String, 4=Bool...
    int var_level = 0;          // 技能等级/变体（仅 w3a/w3d/w3q）
    int data_pointer = 0;       // 数据指针 A-H（仅 w3a）
    std::string value;          // 字符串形式的值
    ObjectID end_id;            // 结束标记
};

struct ObjectEntry {
    ObjectID original_id;       // 原始对象 ID（如 'hfoo'）
    ObjectID new_id;            // 自定义对象 ID（如 'A000'），原始表=0
    std::vector<Modification> modifications;
};

struct ObjectFile {
    uint32_t version = 1;
    std::vector<ObjectEntry> original;   // 标准 Blizzard 对象
    std::vector<ObjectEntry> custom;     // 用户创建的自定义对象
};
```

### 地形文件 (w3e)

```cpp
struct TilePoint {
    float height;           // 地面高度（0x2000 = 地面 0）
    float water_height;     // 水面高度
    bool   map_edge;       // 地图边界
    uint8_t flags;          // 斜坡/枯萎/水域/边界
    uint8_t ground_texture; // 地面纹理索引（4 bit）
    uint8_t texture_detail;
    uint8_t cliff_texture;  // 悬崖纹理索引（4 bit）
    uint8_t layer_height;   // 层高（4 bit）
};

struct Terrain {
    uint32_t version;           // = 11
    char tileset;               // 地块字符（A=Ashenvale, L=Lordaeron...）
    bool custom_tileset;
    std::vector<std::string> ground_tiles; // [Ldrt, Ldro, ...]
    std::vector<std::string> cliff_tiles;  // [CLdi, CLgr, ...]
    int32_t tile_width;         // = map_width + 1
    int32_t tile_height;        // = map_height + 1
    float center_offset_x;
    float center_offset_y;
    std::vector<TilePoint> tilepoints;  // tile_width × tile_height, row-major
};
```

### 单位摆放 (war3mapUnits.doo)

```cpp
struct PlacedUnit {
    std::string type_id;    // 4 字符单位 ID（如 'hfoo'）
    int32_t variation;
    float x, y, z;          // 坐标
    float rotation;
    float scale_x, scale_y, scale_z;
    std::string type_id2;   // 再次类型 ID
    uint8_t flags;
    int32_t owner;          // 0=玩家1, 16=中立被动
    int32_t hp, mp;
    // ... 英雄属性、背包、技能等
};

struct PlacedUnits {
    uint32_t version, subversion;
    std::vector<PlacedUnit> units;
};
```

### 装饰物 (war3map.doo)

```cpp
struct PlacedDoodad {
    std::string type_id;    // 4 字符装饰物 ID（如 'LTbr'）
    int32_t variation;
    float x, y, z;
    float rotation;
    float scale_x, scale_y, scale_z;
    uint8_t flags;          // 0=不可见, 1=可见无碰撞, 2=正常
    uint8_t life_percent;
    int32_t item_table_ptr;
    std::vector<DropItemSet> item_sets;
    int32_t editor_id;
};

struct PlacedDoodads {
    uint32_t version, subversion;
    std::vector<PlacedDoodad> doodads;
    std::vector<SpecialDoodad> special;
};
```

## 开发环境

### 依赖

- **CMake** 3.20+
- **Qt 6.x** (Widgets + OpenGL + OpenGLWidgets)
- **C++17** 编译器 (推荐 MinGW GCC 13+)
- **StormLib** (通过 FetchContent 自动拉取 v9.24)

### 项目结构

```
w3x-packer/
├── src/core/
│   ├── plugin.h              ← IEditorPlugin 接口（插件需要引用）
│   ├── plugin_registry.h/cpp ← PluginRegistry 单例
│   ├── map_builder.h/cpp     ← MapBuilder 编排器
│   ├── metadata.h/cpp        ← MetaDataDB (SLK 字段 + 对象名称)
│   ├── w3i.h/cpp, w3u.h/cpp  ← 格式解析器
│   ├── w3e.h/cpp             ← 地形解析器
│   ├── doo.h/cpp             ← 装饰物格式
│   ├── units_doo.h/cpp       ← 单位格式
│   └── ...                    ← 其他格式
├── gui/                       ← 内置编辑器插件包装
│   ├── mapinfopage.h/cpp     ← MapInfoPlugin (ProvidesTab|NeedsSavable)
│   ├── filebrowser.h/cpp     ← FileBrowserPlugin (ProvidesTab)
│   ├── objecteditor.h/cpp    ← ObjectEditorPlugin (ProvidesTab|NeedsSavable)
│   ├── terraineditor.h/cpp   ← TerrainEditorPlugin (ProvidesTab)
│   ├── placementeditor.h/cpp ← PlacementEditorPlugin (ProvidesTab)
│   └── settingsdialog.h/cpp  ← 设置对话框
├── plugins/                   ← 外部插件 DLL 构建
│   ├── CMakeLists.txt
│   └── sample_tab/           ← 示例插件
└── dist/GUI/plugins/          ← 构建产物：插件 DLL 目录
```

### CMake 配置

根项目的 `CMakeLists.txt` 已配置好 `add_subdirectory(plugins)`（仅在 `WITH_GUI=ON` 时启用）。你只需在 `plugins/CMakeLists.txt` 中添加你的插件目标。

### 插件目录

- 默认：`{exe_dir}/plugins/`
- 可通过 GUI 的 **设置 → 插件目录** 修改
- 设置持久化在 QSettings 中

## 最佳实践

1. **生命周期管理**：在 `init` 中创建所有 QWidget，在 `destroy` 中释放。不要在 `activate`/`deactivate` 中创建/销毁控件。
2. **错误处理**：`activate()` 中使用 try-catch 包裹 builder 调用，地图可能缺少某些文件。
3. **性能**：避免在 `sync_to_builder()` 中执行耗时操作（它阻塞保存流程）。
4. **线程安全**：所有插件方法均在主线程调用，无需额外同步。
5. **QObject 集成**：如果你的插件需要使用信号/槽，继承 `QObject` 和 `IEditorPlugin`：

```cpp
class MyPlugin : public QObject, public IEditorPlugin {
    Q_OBJECT
    // ...
};
```

## 插件调试

1. 编译插件 DLL（确保使用与 w3x-packer 相同的 Qt 和编译器版本）
2. 将 DLL 放入插件目录
3. 启动 w3x-gui.exe，查看状态栏是否有加载信息
4. 检查 `PluginRegistry::load_directory()` 的返回值

### 常见问题

| 问题 | 可能原因 |
|------|----------|
| 插件 DLL 无法加载 | Qt/编译器版本不匹配；DLL 依赖缺失（用 Dependencies Walker 检查）|
| 插件标签页灰色 | `activate()` 未正确执行；地图缺少必要文件导致异常 |
| `w3x_plugin_create` 未找到 | 导出符号未正确声明；检查 DLL 导出表 |
| 保存时崩溃 | `sync_to_builder()` 中访问了空指针或无效数据 |

### 检查 DLL 导出

```bash
# MinGW
objdump -x plugin.dll | grep w3x_plugin_create

# MSVC
dumpbin /EXPORTS plugin.dll
```
