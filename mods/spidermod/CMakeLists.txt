cmake_minimum_required(VERSION 3.15)
project(spidermod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# MinHook
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        master  # release tags lack CMakeLists.txt; master is stable
)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
)

FetchContent_MakeAvailable(minhook imgui)

# ImGui static library (upstream has no CMakeLists)
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx9.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# DirectX 9 SDK
set(DXSDK "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)")

# ---- spidermod.asi ----
add_library(spidermod SHARED
    src/dllmain.cpp
    src/d3d_hook.cpp
    src/menu.cpp
    src/menu_common.cpp
    src/menu_characters.cpp
    src/menu_debug.cpp
    src/menu_engine.cpp
    src/registry_redirect.cpp
    src/freecam.cpp
    src/sector.cpp
)
target_include_directories(spidermod PRIVATE
    src
    "${DXSDK}/Include"
)
target_link_directories(spidermod PRIVATE
    "${DXSDK}/Lib/x86"
)
target_link_libraries(spidermod PRIVATE
    minhook
    imgui_lib
    d3d9
)
set_target_properties(spidermod PROPERTIES
    SUFFIX ".asi"
    OUTPUT_NAME "spidermod"
)

# ---- dinput8.dll proxy ----
add_library(dinput8_proxy SHARED
    src/dinput8_proxy.cpp
)
target_sources(dinput8_proxy PRIVATE src/dinput8_proxy.def)
set_target_properties(dinput8_proxy PROPERTIES
    OUTPUT_NAME "dinput8"
)
