# ── Plugin DLLs ──
# Each plugin is a shared library loaded at runtime by PluginRegistry.
# They link against w3x_core for types and against Qt for the IEditorPlugin interface.
#
# Built only when WITH_GUI=ON (plugins need Qt and are GUI-only).

set(CMAKE_AUTOMOC ON)

# ── Sample Tab Plugin ──
add_library(sample_tab SHARED
    sample_tab/sample_tab.cpp
)

target_include_directories(sample_tab PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sample_tab PRIVATE w3x_core Qt::Widgets)

# Set output directory so build.ps1 can find the DLL
set_target_properties(sample_tab PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

# ── Terrain Edit Plugin (3D terrain editor with independent window) ──
add_library(terrain_edit SHARED
    terrain_edit/terrain_edit_plugin.cpp
    terrain_edit/terrain_edit_window.cpp
    terrain_edit/terrain_edit_widget.cpp
    terrain_edit/blp_reader.cpp
)

target_include_directories(terrain_edit PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(terrain_edit PRIVATE w3x_core Qt::Widgets Qt::OpenGLWidgets)

# Compile .ts → .qm using lrelease
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/plugins/terrain_edit_zh_CN.qm"
    COMMAND Qt6::lrelease -compress
        "${CMAKE_CURRENT_SOURCE_DIR}/terrain_edit/translations/terrain_edit_zh_CN.ts"
        -qm "${CMAKE_BINARY_DIR}/plugins/terrain_edit_zh_CN.qm"
    MAIN_DEPENDENCY terrain_edit/translations/terrain_edit_zh_CN.ts
    COMMENT "Compiling plugin translations (zh_CN)"
)
# Add the generated .qm as a known source so Ninja tracks the dependency
target_sources(terrain_edit PRIVATE "${CMAKE_BINARY_DIR}/plugins/terrain_edit_zh_CN.qm")

set_target_properties(terrain_edit PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
