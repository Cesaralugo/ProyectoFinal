cmake_minimum_required(VERSION 3.16)
project(gui_engine_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output executables land in latency_testing/bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/../bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/../bin")

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# ── nlohmann/json (header-only) ───────────────────────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests  OFF CACHE INTERNAL "")
set(JSON_Install     OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# ── GLFW3 ─────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.10
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# ── ImGui ─────────────────────────────────────────────────────────────────────
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

if(WIN32)
    target_link_libraries(imgui_lib PUBLIC opengl32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(imgui_lib PUBLIC OpenGL::GL)
endif()

# ── Main executable ────────────────────────────────────────────────────────────
set(SOURCES
    main.cpp
    gui.cpp
    preset_manager.cpp
    fft_processor.cpp
    app_receiver.cpp
)

add_executable(gui_engine_cpp ${SOURCES})

target_include_directories(gui_engine_cpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gui_engine_cpp PRIVATE
    imgui_lib
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(gui_engine_cpp PRIVATE ws2_32)
    # Uncomment next line to hide the console window when shipping
    # set_target_properties(gui_engine_cpp PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_compile_options(gui_engine_cpp PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -O2>
    $<$<CXX_COMPILER_ID:MSVC>:/W3 /O2>
)
