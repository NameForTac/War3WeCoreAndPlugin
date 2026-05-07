cmake_minimum_required(VERSION 3.20)
project(w3x-packer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ============================================================
# Dependencies
# ============================================================
include(FetchContent)

FetchContent_Declare(
  stormlib
  GIT_REPOSITORY https://github.com/ladislav-zezula/StormLib.git
  GIT_TAG        v9.24
  GIT_SHALLOW    TRUE
)

# Optionally override with local copy
if(W3X_STORMLIB_DIR)
  FetchContent_Declare(stormlib SOURCE_DIR "${W3X_STORMLIB_DIR}")
endif()

set(STORMLIB_BUILD_TESTING  OFF CACHE BOOL "" FORCE)

# Build StormLib as a shared library (DLL)
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(stormlib)

# ============================================================
# Library
# ============================================================
add_library(w3x_core STATIC
  src/core/buffer.cpp
  src/core/w3i.cpp
  src/core/w3u.cpp
  src/core/w3o.cpp
  src/core/w3c.cpp
  src/core/w3r.cpp
  src/core/imp.cpp
  src/core/wts.cpp
  src/core/slk.cpp
  src/core/archive.cpp
  src/core/w3e.cpp
  src/core/doo.cpp
  src/core/units_doo.cpp
  src/core/map_builder.cpp
  src/core/metadata.cpp
  src/core/object_id_gen.cpp
)

target_include_directories(w3x_core PUBLIC src)
target_link_libraries(w3x_core PUBLIC storm)

# ============================================================
# CLI
# ============================================================
add_executable(w3x_packer src/main.cpp)
target_link_libraries(w3x_packer PRIVATE w3x_core)

# ── Suppress StormLib STORMLIB_NO_AUTO_LINK macro warnings ──
# StormLib defines this to prevent auto-linking in MSVC.
# Our explicit target_link_libraries makes it redundant.
target_compile_definitions(w3x_core PRIVATE _CRT_SECURE_NO_WARNINGS)

# ============================================================
# Tests
# ============================================================
enable_testing()

add_executable(w3x_tests
  tests/test_main.cpp
  tests/test_buffer.cpp
  tests/test_w3i.cpp
  tests/test_w3u.cpp
  tests/test_w3e.cpp
  tests/test_doo.cpp
  tests/test_units_doo.cpp
  tests/test_wts.cpp
  tests/test_slk.cpp
  tests/test_archive.cpp
  tests/test_object_id_gen.cpp
)
target_link_libraries(w3x_tests PRIVATE w3x_core)

add_test(NAME w3x_tests COMMAND w3x_tests)

# ============================================================
# Optional Qt GUI
# ============================================================
option(WITH_GUI "Build Qt GUI application" OFF)

if(WITH_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
    add_subdirectory(gui)
    add_subdirectory(plugins)
endif()

# Copy storm.dll alongside each executable (shared library builds only)
if(BUILD_SHARED_LIBS)
  foreach(tgt w3x_packer w3x_tests)
    add_custom_command(TARGET ${tgt} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:storm>
        $<TARGET_FILE_DIR:${tgt}>
      COMMENT "Copying storm.dll to ${tgt} directory")
  endforeach()
endif()
