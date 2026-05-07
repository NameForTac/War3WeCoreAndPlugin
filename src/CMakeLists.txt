# ── 核心库 ──────────────────────────────────────────
file(GLOB CORE_SRC core/*.cpp)
add_library(w3x_core ${CORE_SRC})
target_include_directories(w3x_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${STORM_INCLUDE_DIR})
target_link_libraries(w3x_core PUBLIC ${STORM_LIB})

# ── CLI 可执行文件 ──────────────────────────────────
add_executable(w3x-pack main.cpp)
target_link_libraries(w3x-pack PRIVATE w3x_core)
