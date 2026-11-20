// platform/GlfwApplication.cpp - GLFW + OpenGL3 window hosting the ImGui UI.
//
// Windows is the shipping target for this milestone. Other platforms get a clear
// message instead of a broken build.
#include "astra/platform/Application.h"

#include <cstdio>
#include <string>

#if defined(_WIN32)

#include <windows.h>
#include <GL/gl.h>

#include <GLFW/glfw3.h>

#include "astra/app/AstraApp.h"
#include "astra/app/KeyForgeApp.h"
#include "astra/core/Log.h"
#include "astra/core/Paths.h"
#include "astra/core/Version.h"
#include "astra/ui/Branding.h"
#include "astra/ui/Theme.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace astra::platform {
namespace {

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

void GlfwErrorCallback(int error, const char* description) {
    log::Error("glfw", std::string("error ") + std::to_string(error) + ": " + description);
}

// Uploads every decoded image into a GL texture so ImGui can draw it.
void InstallGlTextureUploader() {
    ui::branding::SetTextureUploader([](ui::branding::Image& image) {
        GLuint texture = 0;
        glGenTextures(1, &texture);
        if (texture == 0) return;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, image.rgba.data());
        image.texture = (ImTextureID)(intptr_t)texture;  // works whether ImTextureID is ImU64 or void*
    });
}

struct WindowConfig {
    const char* title;
    int width;
    int height;
};

// Creates the GLFW window + ImGui context. Returns nullptr on failure.
GLFWwindow* CreateWindow(const WindowConfig& config, bool& outGlfwReady) {
    glfwSetErrorCallback(GlfwErrorCallback);
    outGlfwReady = glfwInit() == GLFW_TRUE;
    if (!outGlfwReady) {
        log::Error("app", "GLFW could not initialise");
        return nullptr;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(config.width, config.height, config.title, nullptr, nullptr);
    if (!window) {
        log::Error("app", "could not create the Astra window");
        return nullptr;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetWindowSizeLimits(window, 960, 640, GLFW_DONT_CARE, GLFW_DONT_CARE);

    // Centre on the primary monitor's work area.
    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor()) {
        int mx = 0, my = 0, mw = 0, mh = 0;
        glfwGetMonitorWorkarea(monitor, &mx, &my, &mw, &mh);
        if (mw > 0 && mh > 0) {
            glfwSetWindowPos(window, mx + (mw - config.width) / 2, my + (mh - config.height) / 2);
        }
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // Astra manages its own layout state
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    float scaleX = 1.0f;
    float scaleY = 1.0f;
    glfwGetWindowContentScale(window, &scaleX, &scaleY);
    const float uiScale = scaleX > 0.0f ? scaleX : 1.0f;

    if (!ui::LoadFonts(uiScale)) {
        log::Warn("app", "bundled fonts missing - falling back to the ImGui default font");
    }
    ui::ApplyStyle();
    InstallGlTextureUploader();
    ui::branding::Logo();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");
    return window;
}

void DestroyWindow(GLFWwindow* window, bool glfwReady) {
    if (window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    if (glfwReady) glfwTerminate();
}

template <typename DrawFn, typename QuitFn>
int RunLoop(GLFWwindow* window, DrawFn draw, QuitFn shouldQuit) {
    glfwShowWindow(window);

    while (!glfwWindowShouldClose(window) && !shouldQuit()) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        draw();
        ImGui::Render();

        int displayW = 0;
        int displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.035f, 0.035f, 0.043f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    return 0;
}

}  // namespace

int RunStudioWindow() {
    const WindowConfig config{"Astra Studio", 1440, 900};
    bool glfwReady = false;
    GLFWwindow* window = CreateWindow(config, glfwReady);
    if (!window) {
        DestroyWindow(nullptr, glfwReady);
        return 1;
    }

    app::AstraApp studio;
    studio.Init(false);

    const int result = RunLoop(window, [&studio] { studio.Draw(); },
                               [&studio] { return studio.wantsQuit; });

    studio.Shutdown();
    DestroyWindow(window, glfwReady);
    return result;
}

int RunKeyForgeWindow() {
    const WindowConfig config{"Astra KeyForge - owner tool", 1180, 780};
    bool glfwReady = false;
    GLFWwindow* window = CreateWindow(config, glfwReady);
    if (!window) {
        DestroyWindow(nullptr, glfwReady);
        return 1;
    }

    app::KeyForgeApp keyForge;
    keyForge.Init();

    const int result = RunLoop(window, [&keyForge] { keyForge.Draw(); },
                               [&keyForge] { return keyForge.wantsQuit; });

    DestroyWindow(window, glfwReady);
    return result;
}

}  // namespace astra::platform

#else  // !_WIN32

namespace astra::platform {

int RunStudioWindow() {
    std::fprintf(stderr,
                 "Astra Studio's windowed build targets Windows. Use the headless tools on this "
                 "platform: astra_selfcheck and astra_uipreview.\n");
    return 2;
}

int RunKeyForgeWindow() {
    std::fprintf(stderr,
                 "Astra KeyForge's windowed build targets Windows. Build on Windows, or use "
                 "'AstraKeyForge.exe --issue-code' from the packaged zip.\n");
    return 2;
}

}  // namespace astra::platform

#endif  // _WIN32
