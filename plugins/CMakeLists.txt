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
