#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include "Engine.hpp"
#include "../utils/Logger.hpp"
#include "../core/Workspace.hpp"
#include "../ui/UIManager.hpp"
#include "../ui/app_icon.hpp"

// Panels
#include "../ui/panels/EditorPanel.hpp"
#include "../ui/panels/FileExplorerPanel.hpp"
#include "../ui/panels/OtherPanels.hpp"

// ImGui & GLFW
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <dwmapi.h>
#endif

#include <iostream>

#if defined(_MSVC_LANG)
#define FORGE_CXX_LANG _MSVC_LANG
#else
#define FORGE_CXX_LANG __cplusplus
#endif

#if (FORGE_CXX_LANG >= 201703L) && __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

namespace forge {

Engine::Engine() {
}

Engine::~Engine() {
    shutdown();
}

bool Engine::init(int width, int height, const std::string& title) {
    windowWidth = width;
    windowHeight = height;
    windowTitle = title;

    // Initialize Logger
    fs::create_directories("./logs");
    Logger::getInstance().init("./logs/forge.log");
    FORGE_LOG_INFO("Engine", "Initializing code ZEN engine...");

    // Initialize GLFW
    if (!glfwInit()) {
        FORGE_LOG_ERROR("Engine", "Failed to initialize GLFW.");
        return false;
    }

    // Set OpenGL 3.3 Core Profile hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    window = glfwCreateWindow(windowWidth, windowHeight, windowTitle.c_str(), nullptr, nullptr);
    if (!window) {
        FORGE_LOG_ERROR("Engine", "Failed to create GLFW window.");
        glfwTerminate();
        return false;
    }

    // Set dark title bar for Windows native window frame
#if defined(_WIN32)
    HWND hwnd = glfwGetWin32Window(window);
    if (hwnd) {
        BOOL useDarkMode = TRUE;
        // Attribute 20 is DWMWA_USE_IMMERSIVE_DARK_MODE in Windows 10/11
        DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
    }
#endif

    // Set GLFW window icon
    GLFWimage images[3];
    images[0].width = forge::app_icon_48_width;
    images[0].height = forge::app_icon_48_height;
    images[0].pixels = const_cast<unsigned char*>(forge::app_icon_48_rgba);

    images[1].width = forge::app_icon_32_width;
    images[1].height = forge::app_icon_32_height;
    images[1].pixels = const_cast<unsigned char*>(forge::app_icon_32_rgba);

    images[2].width = forge::app_icon_16_width;
    images[2].height = forge::app_icon_16_height;
    images[2].pixels = const_cast<unsigned char*>(forge::app_icon_16_rgba);

    glfwSetWindowIcon(window, 3, images);

    glfwMakeContextCurrent(window);
    
    // Enable VSync (caps frames at monitor refresh rate, e.g. 144 FPS)
    glfwSwapInterval(1);

    // Initialize Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Enable Docking and Multi-viewport (multi-monitor support)
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Direct ini file path to a persistent location relative to the executable's directory
#if defined(_WIN32)
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir = std::string(exePath).substr(0, std::string(exePath).find_last_of("\\/"));
    static std::string iniPath = exeDir + "\\imgui.ini";
    io.IniFilename = iniPath.c_str();

    // Load JetBrains Mono as the default font
    std::string fontPath = "";
    std::vector<std::string> fontPaths = {
        exeDir + "\\resources\\JetBrainsMono-Regular.ttf",
        exeDir + "\\..\\..\\resources\\JetBrainsMono-Regular.ttf",
        "resources/JetBrainsMono-Regular.ttf",
        "../resources/JetBrainsMono-Regular.ttf"
    };
    for (const auto& p : fontPaths) {
        if (fs::exists(p)) {
            fontPath = p;
            break;
        }
    }
    if (!fontPath.empty()) {
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 14.5f);
    }
#endif

    // Setup ImGui bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Initialize systems
    UIManager::getInstance().init();

    // Register all default panels
    UIManager::getInstance().addPanel(std::make_shared<FileExplorerPanel>());
    UIManager::getInstance().addPanel(std::make_shared<EditorPanel>());
    UIManager::getInstance().addPanel(std::make_shared<TerminalPanel>());
    UIManager::getInstance().addPanel(std::make_shared<OutputPanel>());

    // Auto-load last opened folder if saved, else fall back to current directory
    std::string startFolder = fs::current_path().string();
#if defined(_WIN32)
    char startExePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, startExePath, MAX_PATH) != 0) {
        std::string exeDir = std::string(startExePath).substr(0, std::string(startExePath).find_last_of("\\/"));
        std::string settingsPath = exeDir + "\\settings.txt";
        std::ifstream settingsFile(settingsPath);
        if (settingsFile.is_open()) {
            std::string savedFolder;
            std::getline(settingsFile, savedFolder);
            settingsFile.close();
            if (!savedFolder.empty() && fs::exists(savedFolder) && fs::is_directory(savedFolder)) {
                startFolder = savedFolder;
            }
        }
    }
#endif
    Workspace::getInstance().openProject(startFolder);

    isRunning = true;
    FORGE_LOG_INFO("Engine", "code ZEN engine started successfully.");
    return true;
}

void Engine::run() {
    double lastFrameTime = glfwGetTime();

    while (isRunning && !glfwWindowShouldClose(window)) {
        double currentFrameTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentFrameTime - lastFrameTime);
        lastFrameTime = currentFrameTime;

        // Poll system events
        glfwPollEvents();

        // Update systems
        update(deltaTime);

        // Render graphics
        render();
    }
}

void Engine::update(float deltaTime) {
    // Perform updates if needed
}

void Engine::render() {
    // Begin frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Render panels and layout
    UIManager::getInstance().renderAll();

    // End frame
    ImGui::Render();
    
    // Clear screen with slate backdrop
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.09f, 0.09f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update multi-viewport platform windows (multi-monitor undocked windows)
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(window);
}

void Engine::shutdown() {
    if (isRunning) {
        FORGE_LOG_INFO("Engine", "Shutting down code ZEN engine...");
        
        UIManager::getInstance().saveSettings();
        Workspace::getInstance().closeProject();
        UIManager::getInstance().shutdown();

        // Explicitly write layout to persistent storage before shutting down contexts
        ImGuiIO& io = ImGui::GetIO();
        if (io.IniFilename) {
            ImGui::SaveIniSettingsToDisk(io.IniFilename);
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (window) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();

        isRunning = false;
        FORGE_LOG_INFO("Engine", "Shutdown complete.");
    }
}

} // namespace forge
