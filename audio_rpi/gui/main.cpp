/*
 * main.cpp  –  Entry point for the C++ ImGui MultiFX GUI.
 *
 * Initialises GLFW + OpenGL 3 context, sets up ImGui, then runs the
 * render loop calling AudioGui::render() every frame.
 *
 * Usage:  gui_engine.exe [host] [port]
 *   host  – audio engine TCP address (default 127.0.0.1)
 *   port  – audio engine TCP port    (default 54321)
 *
 * Build:  see CMakeLists.txt
 */

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#  include <winsock2.h>
#  pragma comment(lib, "ws2_32.lib")
#endif

#include "gui.hpp"

/* ── GLFW error callback ──────────────────────────────────────────────────── */
static void glfw_error(int code, const char* desc) {
    fprintf(stderr, "[GLFW] Error %d: %s\n", code, desc);
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(int argc, char** argv) {
    /* Parse optional command-line args */
    std::string host = "127.0.0.1";
    int         port = SC_AUDIO_PORT;
    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = std::atoi(argv[2]);

#ifdef _WIN32
    /* Initialise Winsock once for the whole process */
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[main] WSAStartup failed\n");
        return 1;
    }
#endif

    /* ── GLFW ── */
    glfwSetErrorCallback(glfw_error);
    if (!glfwInit()) {
        fprintf(stderr, "[main] glfwInit failed\n");
        return 1;
    }

    /* OpenGL 3.3 core profile */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(
        900, 700,
        "MultiFX Processor  [C++ ImGui]",
        nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "[main] Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); /* VSync – cap at display refresh rate */

    /* ── ImGui ── */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr; /* don't write imgui.ini */

    /* Dark theme */
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding  = 3.0f;
    style.GrabRounding   = 3.0f;
    style.Colors[ImGuiCol_WindowBg]  = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_FrameBg]   = ImVec4(0.16f, 0.16f, 0.20f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab]= ImVec4(0.2f, 0.7f, 1.0f, 1.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    /* ── Application ── */
    AudioGui gui;
    gui.init(host, port);

    /* ── Render loop ── */
    double prev_time = glfwGetTime();
    while (!glfwWindowShouldClose(window) && !gui.want_close()) {
        glfwPollEvents();

        double now = glfwGetTime();
        float  dt  = static_cast<float>(now - prev_time);
        prev_time  = now;

        /* Start ImGui frame */
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        /* Draw GUI */
        gui.render(dt);

        /* Render */
        ImGui::Render();

        int fb_w, fb_h;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    /* ── Cleanup ── */
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
