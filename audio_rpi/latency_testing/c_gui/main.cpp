/*
 * main.cpp - ImGui + GLFW3 + OpenGL3 entry point.
 *
 * Initialises the window, ImGui context and runs the frame loop at
 * whatever the monitor's vsync rate is (~60-165 Hz).  The MainGUI object
 * handles all preset/effect/socket logic.
 *
 * Build:
 *   cd audio_rpi/latency_testing/c_gui
 *   mkdir build && cd build
 *   cmake .. -G "MinGW Makefiles"
 *   mingw32-make -j4
 *   cd ../..
 *   bin/gui_engine_cpp.exe
 */

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>
#include <string>

#include "gui.hpp"

/* ── GLFW error callback ─────────────────────────────────────────────────── */
static void glfw_error_cb(int error, const char* desc) {
    std::fprintf(stderr, "[GLFW] Error %d: %s\n", error, desc);
}

/* ── Entry point ─────────────────────────────────────────────────────────── */
int main(int /*argc*/, char** /*argv*/) {
    glfwSetErrorCallback(glfw_error_cb);

    if (!glfwInit()) {
        std::fprintf(stderr, "[main] glfwInit() failed\n");
        return 1;
    }

    /* OpenGL 3.3 core – works on all modern hardware */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(
        1280, 780,
        "MultiFX Processor – C++ (latency_testing)",
        nullptr, nullptr);

    if (!window) {
        std::fprintf(stderr, "[main] glfwCreateWindow() failed\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  /* vsync */

    /* ImGui setup */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    /* Dark style – resembles PyQtGraph dark theme */
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 4.0f;
    style.FrameRounding    = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.Colors[ImGuiCol_WindowBg]   = {0.08f, 0.08f, 0.10f, 1.0f};
    style.Colors[ImGuiCol_ChildBg]    = {0.10f, 0.10f, 0.13f, 1.0f};
    style.Colors[ImGuiCol_FrameBg]    = {0.15f, 0.15f, 0.20f, 1.0f};
    style.Colors[ImGuiCol_Button]     = {0.20f, 0.35f, 0.55f, 1.0f};
    style.Colors[ImGuiCol_ButtonHovered] = {0.25f, 0.45f, 0.70f, 1.0f};
    style.Colors[ImGuiCol_SliderGrab]    = {0.00f, 0.70f, 1.00f, 1.0f};
    style.Colors[ImGuiCol_SliderGrabActive] = {0.00f, 0.90f, 1.00f, 1.0f};

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    /* Application GUI */
    MainGUI gui;

    /* ── Main loop ─────────────────────────────────────────────────────── */
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        gui.render();

        ImGui::Render();

        int fb_w, fb_h;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    /* Cleanup */
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
