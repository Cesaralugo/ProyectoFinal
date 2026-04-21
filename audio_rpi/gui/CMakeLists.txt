# CMakeLists.txt  –  C++ ImGui GUI for MultiFX Processor
#
# Dependencies are fetched automatically via CMake FetchContent:
#   - Dear ImGui  (immediate-mode GUI, v1.91.0)
#   - GLFW3       (window + OpenGL context, v3.4)
#   - nlohmann/json (header-only JSON, v3.11.3)
#
# Build (MinGW on Windows):
#   cmake -S . -B build -G "MinGW Makefiles"
#   cmake --build build --config Release
#
# Build (Linux/macOS with system GLFW installed):
#   cmake -S . -B build
#   cmake --build build --config Release
#
# Output: build/gui_engine.exe  (Windows) or build/gui_engine (Linux/macOS)

cmake_minimum_required(VERSION 3.16)
project(gui_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch dependencies ────────────────────────────────────────────────────────
include(FetchContent)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.0
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# GLFW3
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# nlohmann/json (header-only)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ── OpenGL (system library) ───────────────────────────────────────────────────
find_package(OpenGL REQUIRED)

# ── ImGui sources ─────────────────────────────────────────────────────────────
set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# ── Application sources ───────────────────────────────────────────────────────
set(APP_SOURCES
    main.cpp
    gui.cpp
    preset_model.cpp
    socket_client.cpp
    app_receiver.cpp
)

add_executable(gui_engine ${APP_SOURCES} ${IMGUI_SOURCES})

target_include_directories(gui_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(gui_engine PRIVATE
    glfw
    OpenGL::GL
    nlohmann_json::nlohmann_json
)

# ── Platform-specific ─────────────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(gui_engine PRIVATE ws2_32)
    # Hide the console window in Release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        if(MINGW)
            target_link_options(gui_engine PRIVATE -mwindows)
        else()
            set_target_properties(gui_engine PROPERTIES
                WIN32_EXECUTABLE TRUE
            )
        endif()
    endif()
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(gui_engine PRIVATE GL dl pthread)
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(gui_engine PRIVATE /W3)
else()
    target_compile_options(gui_engine PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
