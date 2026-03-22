cmake_minimum_required(VERSION 3.20)
project(SpiderView LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# Dependencies (auto-downloaded on first build)
# ---------------------------------------------------------------------------

# raylib 5.5 — rendering, window, input, DDS textures, mesh API
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# Dear ImGui (docking branch) — tool UI with dockable panels
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# rlImGui — raylib ↔ Dear ImGui integration
FetchContent_Declare(rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(rlimgui)
if(NOT rlimgui_POPULATED)
    FetchContent_Populate(rlimgui)
endif()

# ---------------------------------------------------------------------------
# Libraries
# ---------------------------------------------------------------------------

# ImGui static library
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# rlImGui static library
add_library(rlimgui_lib STATIC
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)
target_include_directories(rlimgui_lib PUBLIC ${rlimgui_SOURCE_DIR})
target_link_libraries(rlimgui_lib PUBLIC raylib imgui_lib)

# zlib for PCW/ZWD decompression (same as the engine uses)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(zlib)
# zlib creates 'zlibstatic' target
target_include_directories(zlibstatic PUBLIC ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})

# ---------------------------------------------------------------------------
# SpiderView
# ---------------------------------------------------------------------------

add_executable(SpiderView
    src/main.cpp
    src/app.cpp
    src/formats.cpp
    src/scene.cpp
    src/vm.cpp
    src/scene/camera.cpp
    src/scene/world_shader.cpp
    src/core/asset_cache.cpp
    src/core/game_fs.cpp
    src/core/format_handlers.cpp
    src/ui/panels.cpp
    src/ui/hex_viewer.cpp
    src/export/obj_writer.cpp
    src/export/fbx_writer.cpp
)
target_include_directories(SpiderView PRIVATE src)
target_link_libraries(SpiderView PRIVATE rlimgui_lib zlibstatic)

# MSVC: suppress C runtime security warnings
if(MSVC)
    target_compile_definitions(SpiderView PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
