# ── Qt GUI executable ──
set(CMAKE_AUTOMOC ON)

add_executable(w3x-gui
    main_gui.cpp
    mainwindow.cpp
    mapinfopage.cpp
    filebrowser.cpp
    objecteditor.cpp
    settingsdialog.cpp
    terraineditor.cpp
    placementeditor.cpp
    terrain_renderer_base.cpp
    ../src/core/plugin_registry.cpp
    ../plugins/terrain_edit/blp_reader.cpp
)

target_include_directories(w3x-gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(w3x-gui PRIVATE w3x_core Qt::Widgets Qt::OpenGLWidgets)

# Copy translation files to output directory
file(COPY "${CMAKE_SOURCE_DIR}/translations/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/translations/"
     FILES_MATCHING PATTERN "*.qm")

# Copy storm.dll alongside the GUI executable (shared library builds only)
if(BUILD_SHARED_LIBS)
  add_custom_command(TARGET w3x-gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:storm>
      $<TARGET_FILE_DIR:w3x-gui>
    COMMENT "Copying storm.dll to w3x-gui directory")
endif()
