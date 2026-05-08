# w3x-packer

《魔兽争霸 III》.w3x 地图打包引擎 + CLI + Qt GUI

## 功能

- **解析/生成** war3map.w3i、w3u/w3t/w3b/w3d/w3a/w3h/w3q、w3e、doo、units.doo、wts、w3c、w3r、imp、slk 等全部 Warcraft III 地图格式
- **重新打包** — 清理冗余数据、校验完整性，支持 OBJ / SLK / LNI 三种输出模式
- **提取** — 将 .w3x 解包到目录
- **图形界面** — 基于 Qt6 的多标签编辑器，支持地图信息、对象编辑、3D 地形浏览、单位/装饰物摆放查看
- **插件系统** — 运行时加载 DLL 插件扩展编辑器功能
- **跨编译器** — 支持 MinGW GCC 和 MSVC 构建

## 快速开始

### 前置依赖

- CMake 3.20+
- C++17 编译器（MinGW GCC 13+ 或 MSVC 2022）
- Qt 6.x（Widgets + OpenGL + OpenGLWidgets）
- Ninja（可选，推荐）

StormLib 通过 CMake FetchContent 自动拉取。

### 构建

**MinGW + Qt6（推荐）：**

```powershell
# 一键构建 + 分发
./build.ps1

# 或手动分步
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="D:/Qt/6.10.2/mingw_64" -DWITH_GUI=ON
cmake --build build
```

**MSVC：**

```bash
cmake -B build_vs -G "Visual Studio 17 2022"
cmake --build build_vs --config Release
```

### 运行测试

```bash
./build/w3x_tests.exe
```

## CLI 用法

```
w3x_packer info    <input.w3x>              # 显示地图信息
w3x_packer list    <input.w3x>              # 列出 MPQ 内文件
w3x_packer pack    <input.w3x> <output.w3x> # 重新打包
w3x_packer extract <input.w3x> <dir>        # 解包到目录
w3x_packer dump    <input.w3x> <file>       # 十六进制转储文件
```

## GUI 编辑器

GUI 程序内置了以下编辑器，每个均为 `IEditorPlugin` 实现：

| 标签页 | 功能 | 可保存 |
|--------|------|--------|
| Map Info | 查看/编辑地图名称、作者、描述、玩家、势力等 | 是 |
| Object Editor | 编辑单位/物品/技能等自定义对象 | 是 |
| Terrain | 3D 地形编辑（OpenGL，高度/纹理笔刷，撤销/重做，BLP 贴图） | 是 |
| Placement | 2D 单位/装饰物摆放俯视图 | 否 |
| File Browser | 浏览 MPQ 内文件列表 | 否 |
| Data Viewer | 插件示例：分门别类展示地图全部数据 | 否 |

## 插件开发

插件通过运行时加载的 DLL 扩展编辑器功能。详细文档见 [PLUGIN_DEV.md](PLUGIN_DEV.md)。

```cpp
extern "C" __declspec(dllexport) IEditorPlugin* w3x_plugin_create() {
    return new MyPlugin();
}
```

## 项目结构

```
w3x-packer/
├── src/
│   ├── main.cpp              # CLI 入口
│   └── core/                 # 核心库 (w3x_core)
│       ├── archive.h/cpp     # StormLib MPQ 封装
│       ├── buffer.h/cpp      # 小端序二进制序列化
│       ├── map_builder.h/cpp # 地图数据编排器
│       ├── metadata.h/cpp    # MetaDataDB (SLK + 对象名称)
│       ├── plugin.h          # IEditorPlugin 接口
│       ├── plugin_registry.h/cpp # 插件注册中心
│       ├── types.h           # 公共类型定义
│       ├── w3i.h/cpp         # war3map.w3i 地图信息
│       ├── w3u.h/cpp         # 对象文件 (w3a/u/t/h/b/d/q)
│       ├── w3e.h/cpp         # 地形文件 w3e
│       ├── doo.h/cpp         # 装饰物格式
│       ├── units_doo.h/cpp   # 单位格式
│       ├── wts.h/cpp         # 字符串表
│       ├── slk.h/cpp         # SLK 电子表格
│       ├── w3c.h/cpp         # 镜头文件
│       ├── w3r.h/cpp         # 区域文件
│       ├── imp.h/cpp         # 导入文件列表
│       ├── w3o.h/cpp         # 组合对象导出
│       └── object_id_gen.h/cpp # 自定义 ID 生成器
├── gui/                      # Qt GUI + 内置插件
│   ├── main_gui.cpp          # GUI 入口
│   ├── mainwindow.h/cpp      # 主窗口
│   ├── mapinfopage.h/cpp     # 地图信息插件
│   ├── filebrowser.h/cpp     # 文件浏览器插件
│   ├── objecteditor.h/cpp    # 对象编辑器插件
│   ├── terraineditor.h/cpp   # 3D 地形编辑插件
│   ├── placementeditor.h/cpp # 摆放编辑插件
│   └── settingsdialog.h/cpp  # 设置对话框
├── plugins/                  # 外部 DLL 插件
│   ├── sample_tab/           # Map Data Viewer 示例
│   └── terrain_edit/         # 地形编辑器插件（独立窗口，BLP 贴图，GL 渲染）
├── tests/                    # 单元测试
└── translations/             # 本地化翻译
```

## 支持的格式

| 文件 | 内容 |
|------|------|
| `war3map.w3i` | 地图信息（名称、作者、玩家、势力、科技等） |
| `war3map.w3u/w3t/w3b/w3d/w3a/w3h/w3q` | 对象编辑器数据 |
| `war3map.w3e` | 地形/地块 |
| `war3mapUnits.doo` | 单位和物品摆放 |
| `war3map.doo` | 装饰物 |
| `war3map.wts` | 触发器字符串表 |
| `war3map.w3c` | 镜头 |
| `war3map.w3r` | 区域 |
| `war3map.imp` | 导入文件列表 |
| `war3map.w3o` | 组合对象导出 |
| `war3map.shd` | 阴影图 |
| `war3map.wpm` | 路径图 |

## 许可

MIT

加群QQ452938112及时响应更多意见PS确实不咋上GIT