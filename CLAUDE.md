# W3x-packer 项目结构

C++ 编写的《魔兽争霸 III》.w3x 地图打包引擎 + CLI + Qt GUI + 单元测试。

**Git 提交规则**: 每次更新/修复/添加功能后，Claude 必须直接提交到 Git 并写上提交说明，不得等待用户手动操作。

**技术栈**: C++17, CMake + Ninja, StormLib (FetchContent), Qt6 (GUI, MinGW)

## 项目结构

```
w3x-packer/
├── CMakeLists.txt            # 根构建定义（库 + CLI + 测试 + GUI）
├── build.ps1                 # PowerShell 构建脚本（Qt6 + MinGW + Ninja）
├── build.bat                 # 批处理包装，调用 build.ps1
├── CLAUDE.md                 # 本文件
├── src/
│   ├── main.cpp              # CLI 入口
│   └── core/                 # 核心库 (w3x_core)
│       ├── types.h           # 公共类型定义
│       ├── buffer.h/cpp      # 二进制序列化
│       ├── w3i.h/cpp         # war3map.w3i
│       ├── w3u.h/cpp         # 对象文件 (w3a/u/t/h/b/d/q)
│       ├── w3o.h/cpp         # 组合对象导出文件
│       ├── w3c.h/cpp         # 镜头文件
│       ├── w3r.h/cpp         # 区域文件
│       ├── imp.h/cpp         # 导入文件列表
│       ├── wts.h/cpp         # war3map.wts
│       ├── slk.h/cpp         # SLK 格式
│       ├── archive.h/cpp     # StormLib MPQ 封装
│       ├── metadata.h/cpp    # MetaDataDB (SLK 字段元数据)
│       ├── object_id_gen.h/cpp # 自定义 ID 生成器
│       ├── map_builder.h/cpp # 编排器
│       ├── plugin.h          # IEditorPlugin 接口 (Qt 依赖)
│       └── plugin_registry.h # PluginRegistry 单例 (声明)
├── gui/                      # Qt GUI 界面 + 内置插件
│   ├── CMakeLists.txt
│   ├── main_gui.cpp          # GUI 入口
│   ├── mainwindow.h/cpp      # 主窗口 (仅管理 PluginRegistry + MetaDataDB)
│   ├── mapinfopage.h/cpp     # 地图信息页插件 (MapInfoPlugin)
│   ├── filebrowser.h/cpp     # 文件浏览器插件 (FileBrowserPlugin)
│   ├── objecteditor.h/cpp    # 对象编辑器插件 (ObjectEditorPlugin)
│   ├── terrain_renderer_base.h/cpp  # 3D 地形渲染基类 (SSBO/GPU/BLP/轨道相机，供内置+插件共用)
│   ├── terraineditor.h/cpp   # 3D 地形编辑器插件 (TerrainEditorPlugin，继承 TerrainRendererBase)
│   ├── placementeditor.h/cpp # 2D 单位/装饰物摆放编辑器插件 (PlacementEditorPlugin)
│   └── settingsdialog.h/cpp  # 设置对话框 (含插件目录 + WC3 数据目录)
├── plugins/                  # 外部插件 DLL 构建
│   ├── CMakeLists.txt
│   ├── sample_tab/           # 样例插件 (Map Statistics)
│   │   ├── sample_tab.h
│   │   └── sample_tab.cpp
│   └── terrain_edit/         # 独立窗口地形编辑器插件 (继承 TerrainRendererBase)
│       ├── terrain_edit_plugin.h/cpp
│       ├── terrain_edit_window.h/cpp
│       ├── terrain_edit_widget.h/cpp
│       ├── terrain_edit_types.h
│       ├── blp_reader.h/cpp
│       └── translations/
├── translations/
│   ├── w3x-packer_zh_CN.ts   # 中文本地化源
│   └── w3x-packer_zh_CN.qm   # 编译后的翻译文件
├── tests/
│   ├── test_main.cpp         # 测试入口
│   ├── test_buffer.cpp
│   ├── test_w3i.cpp
│   ├── test_w3u.cpp
│   ├── test_wts.cpp
│   ├── test_slk.cpp
│   └── test_archive.cpp
├── test_data/                # 测试数据目录（当前为空）
└── dist/                     # 构建产物
    ├── GUI/                  # GUI 程序 + Qt DLL + 翻译
    ├── EXE/                  # CLI 程序
    └── TEST/                 # 测试程序
```

## 构建

### MSVC (Visual Studio 2022)
```bash
cmake -B build_vs -G "Visual Studio 17 2022"
cmake --build build_vs --config Release
./build_vs/tests/Release/w3x_tests.exe
```

### MinGW + Qt6 (开发环境，推荐)
```powershell
# 使用 build.ps1：配置 + 编译 + 分发一步完成
# 在 Qt 6.10.2 + MinGW 13.10 + Ninja 环境下
./build.ps1
```

或手动分步：

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DWITH_GUI=ON
cmake --build build

# 运行测试
./build/w3x_tests.exe
```

build.ps1 自动执行上述步骤并组织 dist/ 目录：
- `dist/GUI/` — `w3x-gui.exe` + windeployqt 输出的 DLL/插件/翻译
- `dist/EXE/` — `w3x_packer.exe`
- `dist/TEST/` — `w3x_tests.exe`

## 核心模块

| 模块 | 文件 | 职责 |
|------|------|------|
| **types** | `core/types.h` | ObjectID, ValueType(0-5), ObjectFileType, Modification, ObjectEntry, ObjectFile, MapInfo 等公共类型 |
| **buffer** | `core/buffer.h/cpp` | 小端序二进制序列化（读写 u8/u16/u32/i32/f32/string/bytes） |
| **w3i** | `core/w3i.h/cpp` | war3map.w3i 读写，支持版本 18-31，含 players/forces/tech/random |
| **w3u** | `core/w3u.h/cpp` | 对象文件格式，统一用于 w3a/u/t/h/b/d/q，支持类型特定的额外 int (w3a=2, w3d/w3q=1) |
| **w3o** | `core/w3o.h/cpp` | 组合对象导出文件格式，包装全部 7 种对象文件 |
| **w3e** | `core/w3e.h/cpp` | war3map.w3e 地形/地块文件读写，TilePoint(7字节) + Terrain 结构 |
| **w3c** | `core/w3c.h/cpp` | war3map.w3c 镜头文件读写 |
| **w3r** | `core/w3r.h/cpp` | war3map.w3r 区域文件读写 |
| **imp** | `core/imp.h/cpp` | war3map.imp 导入文件列表读写 |
| **wts** | `core/wts.h/cpp` | war3map.wts 字符串表，支持引号/多行/无引号值 |
| **slk** | `core/slk.h/cpp` | SLK 电子表格格式读写 |
| **archive** | `core/archive.h/cpp` | StormLib MPQ 封装（PIMPL 模式），含 HM3W 头部读写 |
| **map_builder** | `core/map_builder.h/cpp` | 编排器：打开/读取/修改/保存完整 .w3x 文件；支持 w3i/objects/w3e/wts 缓存 + 覆盖 |
| **metadata** | `core/metadata.h/cpp` | MetaDataDB：加载 SLK 元数据 + *strings.txt 对象名称 + WESTRING 键解析 |
| **plugin** | `core/plugin.h` | IEditorPlugin 纯虚基类，PluginCapability 位域，PluginContext |
| **plugin_registry** | `core/plugin_registry.h/cpp` | 插件注册中心单例，内置 + DLL 插件生命周期管理 |
| **terrain_renderer_base** | `gui/terrain_renderer_base.h/cpp` | 3D 地形渲染抽象基类，QOpenGLWidget + QOpenGLFunctions_4_3_Core；SSBO 实例化渲染、BLP 纹理阵列、3D 轨道相机，含 onBeforePaint/onAfterPaint 虚钩子供派生类定制 |
| **terraineditor** | `gui/terraineditor.h/cpp` | 3D 地形编辑器内置插件 (TerrainEditorPlugin)，继承 TerrainRendererBase，轻量包装提供 load_terrain/clear_terrain |
| **placementeditor** | `gui/placementeditor.h/cpp` | 2D 单位/装饰物摆放编辑器，QPainter 俯视图渲染，玩家颜色点，悬停提示 |
| **doo** | `core/doo.h/cpp` | war3map.doo 装饰物格式解析，PlacedDoodad/DropItemSet/PlacedDoodads 结构 |
| **units_doo** | `core/units_doo.h/cpp` | war3mapunits.doo 单位格式解析，PlacedUnit 含背包/技能/随机数据 |

## CLI 用法

```bash
w3x_packer info  <input.w3x>              # 显示地图信息
w3x_packer list  <input.w3x>              # 列出 MPQ 内文件
w3x_packer pack  <input.w3x> <output.w3x> # 重新打包（清理 + 校验）
w3x_packer extract <input.w3x> <dir>      # 解包 .w3x 到目录
w3x_packer dump  <input.w3x> <file>       # Hex dump MPQ 内指定文件
```

| 子命令 | 功能 |
|--------|------|
| `info` | 读取并打印 war3map.w3i 全部字段（名称、作者、描述、版本、玩家等） |
| `list` | 列出 MPQ 归档内的所有文件 |
| `pack` | 读取 .w3x 并重新打包（默认 OBJ 模式，移除 listfile + attributes） |
| `extract` | 将 .w3x 解包到目录（自动创建子目录） |
| `dump` | 以十六进制转储 MPQ 内指定文件内容（限前 128 字节） |

## 测试

| 测试 | 覆盖内容 |
|------|----------|
| `test_buffer` | Buffer 序列化/反序列化一致性 |
| `test_w3i` | MapInfo 完整往返（版本 31，含玩家和队伍） |
| `test_w3u` | ObjectFile 往返（int/string/real 修改项 + 自定义条目） |
| `test_wts` | WTS 引号/多行/无引号值往返 |
| `test_slk` | SLK 表格基本往返 |
| `test_w3e` | w3e 地形往返（4×4 地块，含高度/水位/纹理/悬崖） |
| `test_doo` | 装饰物往返（含掉落物品表 + 特殊装饰物）|
| `test_units_doo` | 单位往返（含背包/技能/随机单位表）|
| `test_archive` | 创建→写入→读取→验证完整 MPQ 周期 |

## 设计要点

- 使用 `FetchContent` 自动拉取 StormLib v9.24（可通过 `W3X_STORMLIB_DIR` 变量指定本地路径覆盖）
- CLI 不支持从目录打包为 .w3x 功能（pack 子命令仅做 .w3x→.w3x 重新打包）
- 构建默认不启用 GUI（`-DWITH_GUI=ON` 开启 Qt6 依赖）
- Windows-only 代码（使用 `_mkdir`、`_chdir` 等 MSVC/MinGW CRT API）
- **Double-DLL 模式**：`terrain_renderer_base.cpp` 同时编译进 w3x-gui.exe 和地形插件 DLL。运行时加载的插件 DLL 无法链接 exe 符号，因此基类 .cpp 必须在插件 CMake 中额外添加一份编译。见 PLUGIN_DEV.md「地形编辑器插件」章节。

## i18n 约定

- 插件级别的翻译存放在各自插件的 `translations/` 目录内，**不放入 core 或主程序 translations/**
- 所有 UI 字符串必须使用 `tr()` 包裹
- 新增字符串后，更新对应插件的 .ts 文件
- CMake 使用 `lrelease` 自动编译 .ts → .qm，.qm 与插件 DLL 同目录输出
- 插件在 `init()` 中通过 `QTranslator` 按系统 locale 自动加载对应翻译（fallback 到 zh_CN）

## 插件架构

所有编辑器功能均为 `IEditorPlugin` 实现，MainWindow 不感知具体编辑器类型。

内置的地形编辑器 (TerrainEditorPlugin) 和插件的地形编辑器 (TerrainEditWidget) 共享 `TerrainRendererBase` 基类，该基类封装了所有 OpenGL 渲染逻辑（SSBO 实例化、BLP 纹理加载、轨道相机）。

### 层次

```
MainWindow (仅核心：打开/保存/列出文件)
    │
    └── PluginRegistry (单例，管理插件生命周期)
            ├── 内置插件 (编译进 GUI)
            │   ├── MapInfoPlugin            (ProvidesTab | NeedsSavable)
            │   ├── FileBrowserPlugin        (ProvidesTab)
            │   ├── ObjectEditorPlugin       (ProvidesTab | NeedsSavable)
            │   ├── TerrainEditorPlugin      (ProvidesTab，含 TerrainWidget : TerrainRendererBase)
            │   └── PlacementEditorPlugin    (ProvidesTab)
            │
            ├── TerrainRendererBase          ← 抽象基类，共享渲染逻辑
            │   └── TerrainEditWidget (插件，位于 terrain_edit.dll)
            │
            └── 外部插件 DLL (运行时加载)
                ├── sample_tab.dll
                └── terrain_edit.dll          (TerrainEditWindow + TerrainEditWidget)
```

### IEditorPlugin 接口 (`src/core/plugin.h`)

| 方法 | 说明 |
|------|------|
| `name()` / `version()` | 插件元信息 |
| `capabilities()` | 位域：ProvidesTab, ProvidesMenu, ProvidesTool, NeedsSavable |
| `init(PluginContext&)` | 插件初始化，返回 false 则卸载 |
| `activate()` / `deactivate()` | 地图加载/卸载时调用 |
| `destroy()` | 插件销毁 |
| `tab_widget()` / `tab_title()` | 标签页集成 |
| `sync_to_builder()` | 保存前同步数据到 MapBuilder |
| `menu_actions()` / `toolbar_actions()` | 菜单/工具栏扩展 |

### PluginContext

```cpp
struct PluginContext {
    MapBuilder* builder = nullptr;    // 地图数据编排器
    MetaDataDB*  meta_db = nullptr;   // WC3 元数据 (SLK + 对象名称)
    QWidget*    parent_widget = nullptr; // 父窗口
};
```

- `meta_db` 提供字段元数据、对象显示名称、WESTRING 键解析
- 插件在 `init()` 中保存 Context，后续在 `activate()` / `sync_to_builder()` 中使用

### PluginRegistry (`src/core/plugin_registry.h/cpp`)

- 单例模式，管理内置 + DLL 插件生命周期
- **内置插件**：通过 `register_plugin()` 注册已创建的实例
- **DLL 插件**：扫描目录 → `LoadLibrary` → 获取 `w3x_plugin_create()` 工厂函数 → 创建实例
- 生命周期：
  1. `register_plugin()` / `load_directory()` — 注册/加载
  2. `init_all(ctx)` — 初始化所有插件
  3. `activate_all()` — 地图打开时激活
  4. `sync_all()` — 保存前同步 NeedsSavable 插件
  5. `deactivate_all()` — 关闭地图时停用
  6. `destroy_all()` — 卸载时销毁

### MainWindow 集成

- **启动**：注册内置插件 → 加载 DLL → init_all → 添加标签页（置灰）
- **打开地图**：activate_all → 启用所有标签页
- **保存**：sync_all → builder->save()
- **关闭/卸载**：deactivate_all → destroy_all
- **插件目录变更**：reload_all 保留内置插件，仅重新加载 DLL

## W3X 文件格式参考

来源: [W3X Files Format](https://867380699.github.io/blog/2019/05/09/W3X_Files_Format/)

### 基础数据类型

所有多字节整数使用**小端序**。

| 类型 | 大小 | 说明 |
|------|------|------|
| Integer | 4 bytes | 有符号 int |
| Short | 2 bytes | 有符号，高 2 位可用作标志 |
| Float | 4 bytes | IEEE 32-bit 小端 |
| Char | 1 byte | ASCII |
| String | 变长 | null 结尾 UTF-8 |
| Trigger String | 12 bytes | `"TRIGSTR_"` + 数字 ID，在 wts 中查找 |
| Flags | 4 bytes | 32 位位域 |

**颜色代码**: 字符串嵌入 `|c00BBGGRR` 十六进制颜色值。

### W3M/W3X 归档格式

.w3x 文件 = **512 字节头部** + MPQ 归档 + 可选 **260 字节尾部**（官方图认证）。

#### HM3W 头部（固定 512 字节）

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 文件标识 `"HM3W"` |
| 4 | 4 | 未知 (Integer) |
| 8 | String | 地图名称 (null-terminated) |
| Var | 4 | 地图标志 (同 w3i flags 位域) |
| Var+4 | 4 | 最大玩家数 |
| 之后 | 填充 | 0x00 填充至 512 字节 |

#### 尾部（可选，260 字节）

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 签名 `"NGIS"` (sign 反转) |
| 4 | 256 | 认证数据 |

#### 地图标志位域

- 0x0001: 隐藏小地图
- 0x0002: 修改盟友优先级
- 0x0004: 近战地图
- 0x0008: 不缩小大尺寸可玩区域
- 0x0010: 遮罩区域部分可见
- 0x0020: 固定玩家设置
- 0x0040: 使用自定义势力
- 0x0080: 使用自定义科技树
- 0x0100: 使用自定义技能
- 0x0200: 使用自定义升级
- 0x0400: 属性菜单已打开过
- 0x0800: 悬崖岸边显示水波
- 0x1000: 滚动岸边显示水波

#### MPQ 内部文件查找顺序

1. 实际文件系统目录（注册表 `Allow Local Files` 启用时）
2. 地图内 (.w3m/.w3x)
3. `War3Patch.mpq`
4. `War3x.mpq` / `War3xlocal.mpq`
5. `War3.mpq`

某些文件（如 `Units\unitUI.slk`）可从地图内覆盖，而 `TerrainArt\CliffTypes.slk` 等不支持。

#### 可能的内部文件列表

| 文件 | 说明 |
|------|------|
| `war3map.w3e` | 环境/地块文件 |
| `war3map.w3i` | 地图信息 |
| `war3map.wtg` | GUI 触发器名称 |
| `war3map.wct` | 自定义文本触发器 |
| `war3map.wts` | 触发器字符串表 |
| `war3map.j` | JASS2 脚本 |
| `war3map.shd` | 阴影图 |
| `war3mapMap.blp` | 小地图图片 |
| `war3mapMap.tga` | 小地图 TGA |
| `war3mapPreview.tga` | 预览图 |
| `war3map.mmp` | 菜单小地图图标 |
| `war3mapPath.tga` / `war3map.wpm` | 路径图 |
| `war3map.doo` | 装饰物（树木） |
| `war3mapUnits.doo` | 单位和物品 |
| `war3map.w3r` | 区域 |
| `war3map.w3c` | 镜头 |
| `war3map.w3s` | 声音定义 |
| `war3map.w3u` | 自定义单位 |
| `war3map.w3t` | 自定义物品 |
| `war3map.w3b` | 自定义可破坏物 |
| `war3map.w3d` | 自定义装饰物 |
| `war3map.w3a` | 自定义技能 |
| `war3map.w3h` | 自定义魔法效果 |
| `war3map.w3q` | 自定义升级 |
| `war3map.w3o` | 组合对象导出文件 |
| `war3mapMisc.txt` | 游戏性常量 |
| `war3mapSkin.txt` | 游戏界面修改 |
| `war3mapExtra.txt` | 地图属性额外数据 |
| `war3map.imp` | 导入文件列表 |
| `war3mapImported\*.*` | 导入的文件 |
| `war3map.wai` | AI 文件 |
| `(listfile)` | 文件列表 |
| `(signature)` | 签名 |
| `(attributes)` | 属性（CRC32/时间戳） |

### war3map.j — JASS2 脚本

纯文本文件，魔兽争霸地图脚本。游戏创建时调用 `config`，开始后调用 `main`。

**6 种基本类型**: `boolean`, `integer`, `real`, `string`, `code`, `handle`。所有其他类型 (unit, item, timer 等) 继承自 `handle`。

**运算符**: `()` `+` `-` `*` `/` `=` `==` `<` `<=` `>` `>=` `!=` `not` `and` `or` `[]`。`+` 也可拼接字符串。`[]` 访问数组元素。

**整数字面量**: 十进制 `65`、十六进制 `0x3F`、ASCII `'A'`（单位/物品 ID 如 `'Ahbz'`）。

### war3map.w3e — 环境/地块文件

#### 头部

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 文件标识 `"W3E!"` |
| 4 | 4 | 版本 (= 0x0B = v11) |
| 8 | 1 | 主地块字符（见下表） |
| 9 | 4 | 自定义地块标志 (1=自定义) |
| 13 | 4 | 地面地块数 a |
| 17 | 4*a | 地面地块 ID（4 字符，如 `"Ldrt"`） |
| Var | 4 | 悬崖地块数 b |
| Var+4 | 4*b | 悬崖地块 ID（如 `"CLdi"`） |
| Var | 4 | 地图宽度 + 1 = Mx |
| Var+4 | 4 | 地图高度 + 1 = My |
| Var+8 | 4 | 中心偏移 X (float) |
| Var+12 | 4 | 中心偏移 Y (float) |

中心偏移公式: `-1 × (Mx-1) × 128 / 2` 和 `-1 × (My-1) × 128 / 2`。

**地块字符**: A=Ashenvale, B=Barrens, C=Felwood, D=Dungeon, F=Lordaeron Fall, G=Underground, L=Lordaeron Summer, N=Northrend, Q=Village Fall, V=Village, W=Lordaeron Winter, X=Dalaran, Y=Cityscape, Z=Sunken Ruins, I=Icecrown, J=Dalaran Ruins, O=Outland, K=Black Citadel。

#### 瓦片点数据 (Mx × My 个 7 字节块)

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 2 | 地面高度 (Short, 0x2000 = 地面 0) |
| 2 | 2 | 水位 + 边界标志 (bit15=边界) |
| 4 | 0.5 | 标志 (4 bits): 0x1=斜坡, 0x2=枯萎, 0x4=水, 0x8=边界2 |
| 4.5 | 0.5 | 地面纹理类型索引 (4 bits) |
| 5 | 1 | 纹理细节字节 |
| 6 | 0.5 | 悬崖纹理类型 (4 bits) |
| 6.5 | 0.5 | 层高 (4 bits) |

### war3map.shd — 阴影图

无头部。文件大小 = `16 × map_width × map_height`。每字节 `0x00`（无阴影）或 `0xFF`（有阴影）。

### war3map.wpm — 路径图（新格式）

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 文件标识 `"MP3W"` |
| 4 | 4 | 版本 = 0 |
| 8 | 4 | 路径图宽度 = map_width × 4 |
| 12 | 4 | 路径图高度 = map_height × 4 |
| 16 | Var | 数据: (w×4) × (h×4) 字节 |

**每字节标志**: 0x02=不可行走, 0x04=不可飞行, 0x08=不可建造, 0x20=枯萎, 0x40=无水, 0x80=未知。

### war3map.doo — 装饰物

#### 头部 (Frozen Throne)

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 文件标识 `"W3do"` |
| 4 | 4 | 版本 = 8 |
| 8 | 4 | 子版本 (0x0B) |
| 12 | 4 | 装饰物数量 |

#### 每个装饰物 (约 50+ 字节)

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 装饰物 ID |
| 4 | 4 | 变体 (int) |
| 8 | 12 | X/Y/Z 坐标 (float) |
| 20 | 4 | 弧度角度 (float) |
| 24 | 12 | X/Y/Z 缩放 (float) |
| 36 | 1 | 标志 (0=不可见+无碰撞, 1=可见+无碰撞, 2=正常) |
| 37 | 1 | 生命值百分比 (0x64=100%) |
| 38 | 4 | 随机物品表指针 (-1=无, ≥0=w3i 表索引) |
| 42 | 4 | 掉落物品集数 n |
| 46 | 4 | WE 中装饰物 ID 编号 |

**掉落物品集**: 4 字节物品数 d + d × (4 字节物品 ID + 4 字节概率)。

#### 特殊装饰物段

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 版本 (=0) |
| 4 | 4 | 特殊装饰物数 s |
| s×16 | | 每个: 4 字节 ID + 4 字节 Z + 4 字节 X + 4 字节 Y |

### war3mapUnits.doo — 单位和物品

#### 头部

同 war3map.doo: `"W3do"`, 版本=8, 子版本 0x0B, 数量。

#### 每个单位 (变长，约 95+ 字节)

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 类型 ID (`"iDNR"`=随机物品, `"uDNR"`=随机单位) |
| 4 | 4 | 变体 (int) |
| 8 | 12 | X/Y/Z 坐标 (float) |
| 20 | 4 | 弧度角度 (float) |
| 24 | 12 | X/Y/Z 缩放 (float) |
| 36 | 4 | 再次类型 ID |
| 40 | 1 | 标志 (同装饰物) |
| 41 | 4 | 所属玩家 (0=玩家1, 16=中立被动) |
| 45 | 1 | 未知 (0) |
| 46 | 1 | 未知 (0) |
| 47 | 4 | HP (-1=默认) |
| 51 | 4 | MP (-1=默认, 0=无魔法) |
| 55 | 4 | 物品掉落表指针 (-1=无) |
| 59 | 4 | 掉落物品集数 s |
| 63 | 4 | 金钱 (默认 12500) |
| 67 | 4 | 目标获取 (-1=正常, -2=驻守) |
| 71 | 4 | 英雄等级 |
| 75 | 4 | 力量 (0=默认) |
| 79 | 4 | 敏捷 (0=默认) |
| 83 | 4 | 智力 (0=默认) |
| 87 | 4 | 背包物品数 n |
| 91 | 4 | 修改技能数 m |
| 95 | 4 | 随机单位/物品标志 r |

**随机单位/物品标志 r**: r=0 时为 3 字节等级 + 1 字节物品类。r=1 时为 4 字节组号 + 4 字节位置号。r=2 时为 4 字节单位数 + n × (4 字节单位 ID + 4 字节概率)。

**背包物品**: 4 字节槽位(实际-1) + 4 字节物品 ID。

**技能修改**: 4 字节技能 ID + 4 字节自动施放标志 + 4 字节等级。

**末尾**: 4 字节自定义颜色 + 4 字节传送门目标 + 4 字节创建编号。

### war3map.w3i — 地图信息（详细）

#### 头部 (版本 25+)

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 版本 (=25) |
| 4 | 4 | 保存次数 (地图版本) |
| 8 | 4 | 编辑器版本 (小端) |
| 12 | Str | 地图名称 |
| Var | Str | 地图作者 |
| Var | Str | 地图描述 |
| Var | Str | 推荐玩家数 |
| Var | 32 | 8 个 float 镜头边界 (JASS 坐标) |
| Var | 16 | 4 个 int 镜头边界互补值 A/B/C/D |
| Var | 4 | 可玩区域宽 E |
| Var+4 | 4 | 可玩区域高 F |

边界公式: 地图宽 = A + E + B, 地图高 = C + F + D。

| 偏移 | 大小 | 字段 |
|------|------|------|
| Var | 4 | 标志位域 |
| Var+4 | 1 | 主地块类型字符 |
| Var+5 | 4 | 加载屏幕背景编号 (-1=自定义) |
| Var+9 | Str | 自定义加载屏幕模型路径 |
| Var | Str | 加载屏幕文本 |
| Var | Str | 加载屏幕标题 |
| Var | Str | 加载屏幕副标题 |
| Var | 4 | 使用游戏数据集索引 |
| Var | Str | 序幕屏幕路径 |
| Var | Str | 序幕屏幕文本 |
| Var | Str | 序幕屏幕标题 |
| Var | Str | 序幕屏幕副标题 |
| Var | 4 | 地形雾标志 |
| Var | 4 | 雾起始 Z (float) |
| Var+4 | 4 | 雾结束 Z (float) |
| Var+8 | 4 | 雾密度 (float) |
| Var+12 | 4 | 雾颜色 RGBA (4 bytes) |
| Var+16 | 4 | 全局天气 ID (0=无, 否则 4 字符) |
| Var+20 | Str | 自定义声音环境标签 |
| Var | 1 | 自定义光照环境地块 ID |
| Var+1 | 4 | 水色 RGBA (4 bytes) |
| Var+5 | 4 | 最大玩家数 MAXPL |

之后: MAXPL × Player Data + MAXFC × Force Data + UCOUNT × Upgrade Change + TCOUNT × Tech Change + UTCOUNT × Random Unit Table + ITCOUNT × Random Item Table。

#### Player Data

4 字节玩家编号 + 4 字节类型 (1=人, 2=电脑, 3=中立, 4=可救援) + 4 字节种族 + 4 字节标志 + String 名称 + 4 字节起始 X (float) + 4 字节起始 Y (float) + 4 字节低优先级盟友 + 4 字节高优先级盟友。

#### Force Data

4 字节标志 (0x01=结盟, 0x02=结盟胜利, 0x04=共享视野, 0x10=共享单位, 0x20=共享高级控制) + 4 字节玩家掩码 + String 名称。

### war3map.w3u/w3t/w3b/w3d/w3a/w3h/w3q — 对象编辑器数据

通用格式，用于单位/物品/可破坏物/装饰物/技能/魔法效果/升级。

#### 头部

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 版本 (=1) |
| 4 | Var | 原始对象表（标准 Blizzard 对象） |
| Var | Var | 自定义对象表（用户创建） |

#### 表结构

4 字节对象数 n + n × Object Definition。

#### Object Definition

4 字节原始对象 ID + 4 字节新对象 ID（原始表=0）+ 4 字节修改数 m + m × Modification。

#### Modification Structure

4 字节修改 ID + 4 字节变量类型 t + [可选 4 字节等级/变体] + [可选 4 字节数据指针] + 变长值 + 4 字节结束标记 (0 或 对象 ID)。

**变量类型 t**: 0=Integer, 1=Real, 2=Unreal(0-1), 3=String, 4=Bool, 5=Char, 6-21=各种列表/ID。

#### 文件特定细节

| 文件 | 对象类型 | 额外整数 |
|------|----------|----------|
| w3u | 单位 | 无 |
| w3t | 物品 | 无 |
| w3b | 可破坏物 | 无 |
| w3d | 装饰物 | 变体 |
| w3a | 技能 | 等级 + 数据指针 |
| w3h | 魔法效果 | 无 |
| w3q | 升级 | 等级 |

技能数据指针：0=A 到 6=H（对应 AbilityData.slk 的 Data 列）。

### w3o — 组合对象导出文件

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 版本 (=1) |
| 4 | 4 | 是否有单位数据 |
| Var | 变长 | [完整 w3u] |
| 之后 | 4 | 是否有物品数据 |
| Var | 变长 | [完整 w3t] |
| 之后 | 4 | 是否有可破坏物数据 |
| Var | 变长 | [完整 w3b] |
| 之后 | 4 | 是否有装饰物数据 |
| Var | 变长 | [完整 w3d] |
| 之后 | 4 | 是否有技能数据 |
| Var | 变长 | [完整 w3a] |
| 之后 | 4 | 是否有魔法效果数据 |
| Var | 变长 | [完整 w3h] |
| 之后 | 4 | 是否有升级数据 |
| Var | 变长 | [完整 w3q] |

### war3map.wts — 触发器字符串表

纯文本格式：
```
STRING <ID>
{
<string value>
}
```

ID 为数字标识符。地图中的 `TRIGSTR_***` 在此查找对应字符串。只有第一个定义生效。

### war3map.wtg — GUI 触发器名称

#### 头部 (版本 7)

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 文件标识 `"WTG!"` |
| 4 | 4 | 版本 = 7 |
| 8 | 4 | 触发器类别数 a |
| Var | a×Cat | Category Definition |
| Var | 4 | 未知 |
| Var | 4 | 变量数 c |
| Var | c×Var | Variable Definition |
| Var | 4 | 触发器数 d |
| Var | d×Trig | Trigger Definition |

#### Category Definition: 4 字节索引 + String 名称 + 4 字节类型 (0=普通, 1=注释)。

#### Variable Definition: String 名称 + String 类型 + 4 字节未知 + 4 字节数组标志 + 4 字节数组大小 + 4 字节初始化标志 + String 初始值。

#### Trigger Definition: String 名称 + String 描述 + 4 字节类型 + 4 字节启用状态 + 4 字节自定义文本标志 + 4 字节初始状态 + 4 字节地图初始化运行 + 4 字节类别索引 + 4 字节 ECA 函数数 + ECA 函数。

#### ECA Function (Event/Condition/Action): 4 字节类型 (0=事件, 1=条件, 2=动作) + String 名称 + 4 字节启用 + 参数 + 4 字节未知 + 4 字节嵌套 ECA 数 + 嵌套 ECA。

### war3map.wct — 自定义文本触发器

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 版本 = 1 |
| Var | String | 自定义脚本代码注释 |
| Var | 1×CT | 全局脚本 Custom Text Trigger |
| Var | 4 | 触发器数 n |
| Var | n×CT | Custom Text Trigger |

**Custom Text Trigger**: 4 字节大小 s（含 null）+ s 字节文本。未转换 GUI 触发器时大小=0。

### war3map.w3c — 镜头

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 版本 = 0 |
| 4 | 4 | 镜头数 n |
| n× | | 每个: 12 字节 Target X/Y + Z Offset (float) + 4 字节旋转角(度) + 4 字节俯仰角(度) + 4 字节距离 + 4 字节 Roll + 4 字节 FOV + 4 字节 FarZ + 4 字节未知 + String 镜头名称 |

### war3map.w3r — 区域

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 版本 = 5 |
| 4 | 4 | 区域数 n |
| n× | | 每个: 4×4 边界 Left/Right/Bottom/Top (float) + String 名称 + 4 字节索引 + 4 字节天气效果 ID + String 环境声音 + 3 字节颜色 BBGGRR + 1 字节结束标记 |

### war3map.w3s — 声音定义

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 版本 = 1 |
| 4 | 4 | 声音数 n |
| n× | | String ID 名称 + String 文件路径 + String EAX 效果 + 4 字节标志 + 4 字节淡入率 + 4 字节淡出率 + 4 字节音量 + 4 字节音高 (float) + 2×4 字节未知 + 4 字节通道 + 4 字节最小距离 (float) + 4 字节最大距离 (float) + 4 字节距离截止 (float) + 5×4 字节未知 |

未设置的 float 使用 sentinel 值 `0x4F800000`。

### war3map.wai — AI 文件

#### 头部

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 版本 = 2 |
| Var | String | AI 名称 |
| Var | 4 | 种族 |
| Var | 4 | 选项 (HiWord 和 LoWord 位域) |
| Var | 4 | 工人/建筑数 |
| Var | 4 | 采金工人 ID |
| Var+4 | 4 | 采木工人 ID |
| Var+8 | 4 | 基地建筑 ID |
| Var+12 | 4 | 金矿建筑 ID |
| Var+16 | 4 | 条件数 a |
| Var+20 | 4 | 未知 (7) |
| 之后 | a× | Condition Definition |
| 之后 | 4×3 | 第一/二/三英雄 ID |
| 之后 | 6×4 | 英雄训练顺序百分比 |
| 之后 | 90×4 | 英雄技能 ID (9 英雄位 × 10 技能槽) |
| 之后 | 4 | 建造优先级数 c |
| 之后 | c× | Build Priority |
| 之后 | 4 | 采集优先级数 d |
| 之后 | d× | Harvest Priority |
| 之后 | 4 | 目标优先级数 e |
| 之后 | e× | Target Priority |
| 之后 | 4 | 重复波数 |
| 之后 | 4 | 最小部队 |
| 之后 | 4 | 初始延迟 |
| 之后 | 4 | 攻击组数 f |
| 之后 | f× | Attack Group |
| 之后 | 4 | 攻击波数 g |
| 之后 | g× | Attack Wave |
| 之后 | 4 | 未知 (1) |
| 之后 | 4 | 游戏选项 |
| 之后 | 4 | 游戏速度 |
| 之后 | String | 地图文件路径 |
| 之后 | 4 | 玩家数 h (0-2) |
| 之后 | h× | Player |
| 之后 | 4 | 未知 |

### war3map.imp — 导入文件列表

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 版本 |
| 4 | 4 | 文件数 n |
| n× | | 1 字节路径类型 + String 文件路径 |

路径类型 5/8 = 标准 `"war3mapImported\"` 前缀；10/13 = 自定义路径。

### war3mapMisc.txt / war3mapSkin.txt / war3mapExtra.txt

标准 INI 格式文本文件: `[sectionname]` + `key=value`。

### (attributes) — MPQ 属性文件

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 未知 = 0x64000000 |
| 4 | 4 | 未知 = 0x03000000 |
| 之后 | 每文件 4 | CRC32 校验和（按归档顺序） |
| 之后 | 每文件 8 | Filetime (低 + 高, WinAPI FILETIME) |

最后一个 CRC32（属性文件自身）始终为 0。

### SLK 文件 (电子表格格式)

逐行读取，前两个字母为记录类型：

```
ID;PWXL;N;E                              — 文件标识（首行）
B;Y<行>;X<列>;D0 0 Y-1 X-1               — 尺寸
C;Y<行>;X<列>;K"value"                    — 字符串值
C;Y<行>;X<列>;K<number>                   — 数值
```

### BLP 图片文件

#### 公共头部 (156 字节)

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 文件标识 `"BLP1"` |
| 4 | 4 | 类型: 0=JPG-BLP, 1=Paletted |
| 8 | 4 | Alpha 标志: 0x08=有 alpha, 0x00=无 |
| 12 | 4 | 图片宽 |
| 16 | 4 | 图片高 |
| 20 | 4 | Alpha/队伍颜色标志 (3/4/5) |
| 24 | 4 | 始终 0x01 |
| 28 | 64 | 16 个 mipmap 偏移量 |
| 92 | 64 | 16 个 mipmap 大小 |

**JPG-BLP**: 4 字节 JPEG 头部大小 h + h 字节 JPEG 头部 + 填充 + 各 mipmap 的 JPEG 数据。

**Paletted BLP**: 256×4 字节 BGRA 调色板 + 宽×高 字节颜色索引 + 宽×高 字节 alpha 索引（类型≠5 时）。

### W3V/W3Z/W3G — 压缩文件格式 (zlib deflate)

回放文件 (W3G)、存档 (W3Z)、游戏缓存 (W3V) 共用此格式。

#### 头部 (0x40-0x44 字节)

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 28 | 签名 `"Warcraft III recorded game\x1A\0"` |
| 28 | 4 | 头部大小 (0x40/0x44) |
| 32 | 4 | 压缩后大小（含头部） |
| 36 | 4 | 头部版本 |
| 40 | 4 | 解压后原始大小 |
| 44 | 4 | 压缩数据块数 n |
| 48 | 4 | 版本标识 (`"WAR3"` 或 `"W3XP"`) |
| 52 | 4 | 版本号 |
| 56 | 2 | 构建号 |
| 58 | 2 | 标志 (0x0000=单人, 0x8000=多人) |
| 60 | 4 | 时长(ms) 或 0 |
| 64 | 4 | CRC32（计算时此字段置 0） |

#### 压缩数据块 (n 个)

2 字节压缩大小 + 2 字节原始大小(2048 倍数) + 4 字节哈希 + m 字节 zlib 压缩数据。

zlib 头部通常为 `0x78 0x01`。

### W3N 战役文件格式

同 W3M/W3X：512 字节 HM3W 头部 + MPQ + 可选 260 字节尾部。仅 Frozen Throne。

**内部文件**: `war3campaign.w3u`, `.w3t`, `.w3a`, `.w3b`, `.w3d`, `.w3q`, `.w3f`, `.imp`, `war3campaignImported\*.*`。

### war3campaign.w3f — 战役信息文件

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | 版本 (=1) |
| 4 | 4 | 战役版本 (保存次数) |
| 8 | 4 | 编辑器版本 |
| Var | String | 战役名称 |
| Var | String | 战役难度 |
| Var | String | 作者 |
| Var | String | 战役描述 |
| Var | 4 | 标志 (0=Fixed+W3M, 1=Variable+W3M, 2=Fixed+W3X, 3=Variable+W3X) |
| Var | 4 | 背景屏幕索引 |
| Var | String | 自定义背景屏幕路径 |
| Var | String | 小地图图片路径 |
| Var | 4 | 环境声音索引 |
| Var | String | 自定义环境声音路径 |
| Var | 4 | 地形雾标志 + 雾参数 |
| Var | 4 | 光标/UI 种族索引 |
| Var | 4 | 地图数 n |
| 之后 | n× | Map Title Structure |
| 之后 | 4 | 流程图地图数 m |
| 之后 | m× | Map Order Structure |

#### Map Title Structure: 4 字节可见标志 + String 章节标题 + String 地图标题 + String 地图路径。

#### Map Order Structure: String 未知 + String 地图路径。
