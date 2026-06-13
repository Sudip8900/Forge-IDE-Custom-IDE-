#include "UIManager.hpp"
#include "../utils/Logger.hpp"
#include "../core/Workspace.hpp"
#include <imgui.h>
#include <iostream>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#endif

namespace forge {

UIManager& UIManager::getInstance() {
    static UIManager instance;
    return instance;
}

void UIManager::init() {
    loadSettings();
}

void UIManager::shutdown() {
    panels.clear();
}

void UIManager::addPanel(std::shared_ptr<Panel> panel) {
    panels.push_back(panel);
}

void UIManager::renderAll(bool showMenuBar) {
    if (!themeApplied) {
        applyThemeByName(activeThemeName);
        themeApplied = true;
    }

    createMainDockSpace();

    // Render Menu Bar
    if (showMenuBar && ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New File...", "Ctrl+N")) {
#if defined(_WIN32)
                char filename[MAX_PATH] = "";
                OPENFILENAMEA ofn;
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = NULL;
                ofn.lpstrFilter = "C++ Source (*.cpp)\0*.cpp\0C++ Header (*.h;*.hpp)\0*.h;*.hpp\0All Files (*.*)\0*.*\0";
                ofn.lpstrFile = filename;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
                ofn.lpstrDefExt = "cpp";
                
                if (GetSaveFileNameA(&ofn)) {
                    std::ofstream file(filename);
                    file.close();
                    Workspace::getInstance().openDocument(filename);
                    Workspace::getInstance().refreshFileTreeAsync();
                }
#endif
            }
            if (ImGui::MenuItem("Open File...", "Ctrl+O")) {
#if defined(_WIN32)
                char filename[MAX_PATH] = "";
                OPENFILENAMEA ofn;
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = NULL;
                ofn.lpstrFilter = "All Files (*.*)\0*.*\0C++ Files (*.cpp;*.h;*.hpp)\0*.cpp;*.h;*.hpp\0";
                ofn.lpstrFile = filename;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                ofn.lpstrDefExt = "";
                
                if (GetOpenFileNameA(&ofn)) {
                    Workspace::getInstance().openDocument(filename);
                }
#endif
            }
            if (ImGui::MenuItem("Open Project / Folder...")) {
#if defined(_WIN32)
                char path[MAX_PATH] = "";
                BROWSEINFOA bi = { 0 };
                bi.lpszTitle = "Select Project Folder";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
                if (pidl != 0) {
                    SHGetPathFromIDListA(pidl, path);
                    IMalloc* imalloc = 0;
                    if (SUCCEEDED(SHGetMalloc(&imalloc))) {
                        imalloc->Free(pidl);
                        imalloc->Release();
                    }
                    Workspace::getInstance().openProject(path);
                }
#endif
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                // Will be caught by main event loop window state checks
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            for (auto& panel : panels) {
                bool open = panel->isOpen();
                if (ImGui::MenuItem(panel->getName(), nullptr, &open)) {
                    panel->setOpen(open);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Render each panel
    for (auto& panel : panels) {
        if (panel->isOpen()) {
            panel->render();
        }
    }
}

void UIManager::createMainDockSpace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    bool open = true;
    ImGui::Begin("ForgeIDEDockSpaceWindow", &open, window_flags);
    ImGui::PopStyleVar(3);

    // Dockspace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("ForgeIDEDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    }

    ImGui::End();
}

void UIManager::applyThemeByName(const std::string& name) {
    activeThemeName = name;
    if (name == "Cyberpunk 2077") {
        applyCyberpunkTheme();
    } else if (name == "Monokai Classic") {
        applyMonokaiTheme();
    } else if (name == "Github Light") {
        applyGithubLightTheme();
    } else {
        applySlateDarkTheme();
    }
    saveSettings();
}

void UIManager::applySlateDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 4.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.WindowPadding = ImVec2(8.0f, 8.0f);

    // Deep slate color palette
    colors[ImGuiCol_Text]                   = ImVec4(0.88f, 0.91f, 0.94f, 1.00f); // Slate-100
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.45f, 0.49f, 0.54f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.09f, 0.09f, 0.10f, 1.00f); // Slate-900 (deep dark charcoal)
    colors[ImGuiCol_ChildBg]                = ImVec4(0.11f, 0.11f, 0.12f, 1.00f); // Slate-850
    colors[ImGuiCol_PopupBg]                = ImVec4(0.11f, 0.11f, 0.12f, 0.96f);
    colors[ImGuiCol_Border]                 = ImVec4(0.16f, 0.16f, 0.18f, 1.00f); // Slate-750 (subtle grid boundaries)
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    // Headers & Fields
    colors[ImGuiCol_FrameBg]                = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    
    colors[ImGuiCol_TitleBg]                = ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.13f, 0.13f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
    
    // Active tabs, scrolling bars (neon highlights)
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.32f, 0.32f, 0.36f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.40f, 0.40f, 0.44f, 1.00f);
    
    colors[ImGuiCol_CheckMark]              = ImVec4(0.39f, 0.40f, 0.94f, 1.00f); // Neon Indigo
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.39f, 0.40f, 0.94f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]        = ImVec4(0.49f, 0.50f, 0.96f, 1.00f);
    
    // Buttons
    colors[ImGuiCol_Button]                 = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.39f, 0.40f, 0.94f, 1.00f); // Active turns Indigo
    
    // Headers (nodes, lists)
    colors[ImGuiCol_Header]                 = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.23f, 0.23f, 0.26f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.39f, 0.40f, 0.94f, 0.80f);
    
    // Separators
    colors[ImGuiCol_Separator]              = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.39f, 0.40f, 0.94f, 1.00f);
    
    // Tabs (The iconic game engine look)
    colors[ImGuiCol_Tab]                    = ImVec4(0.13f, 0.13f, 0.14f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.23f, 0.23f, 0.26f, 1.00f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.24f, 0.26f, 0.47f, 1.00f); // Slate Indigo Mix
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    
    // Docking highlights
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.39f, 0.40f, 0.94f, 0.50f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);

    editorTheme.text = ImVec4(0.88f, 0.91f, 0.94f, 1.00f);
    editorTheme.keyword = ImVec4(0.33f, 0.61f, 0.83f, 1.00f);
    editorTheme.macro = ImVec4(0.74f, 0.58f, 0.98f, 1.00f);
    editorTheme.type = ImVec4(0.30f, 0.78f, 0.69f, 1.00f);
    editorTheme.string = ImVec4(0.80f, 0.56f, 0.47f, 1.00f);
    editorTheme.comment = ImVec4(0.41f, 0.60f, 0.33f, 1.00f);
    editorTheme.lineNumbers = ImVec4(0.4f, 0.4f, 0.45f, 1.00f);
}

void UIManager::applyCyberpunkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    // Cyberpunk Neon colors
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.00f, 1.00f); // Neon Yellow
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.35f, 0.65f, 1.00f); // Dim Purple
    colors[ImGuiCol_WindowBg]               = ImVec4(0.05f, 0.04f, 0.09f, 1.00f); // Deep Purple/Black
    colors[ImGuiCol_ChildBg]                = ImVec4(0.09f, 0.07f, 0.15f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.09f, 0.07f, 0.15f, 0.96f);
    colors[ImGuiCol_Border]                 = ImVec4(1.00f, 0.00f, 0.50f, 0.60f); // Neon Pink
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    colors[ImGuiCol_FrameBg]                = ImVec4(0.12f, 0.09f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.18f, 0.14f, 0.33f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.24f, 0.19f, 0.44f, 1.00f);
    
    colors[ImGuiCol_TitleBg]                = ImVec4(0.09f, 0.07f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.15f, 0.08f, 0.25f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.05f, 0.04f, 0.09f, 1.00f);
    
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.09f, 0.07f, 0.15f, 1.00f);
    
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.05f, 0.04f, 0.09f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(1.00f, 0.00f, 0.50f, 0.40f); // Pink Grab
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(1.00f, 0.00f, 0.50f, 0.70f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(1.00f, 0.00f, 0.50f, 1.00f);
    
    colors[ImGuiCol_CheckMark]              = ImVec4(0.00f, 1.00f, 1.00f, 1.00f); // Neon Cyan
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]        = ImVec4(0.50f, 1.00f, 1.00f, 1.00f);
    
    colors[ImGuiCol_Button]                 = ImVec4(0.15f, 0.08f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(1.00f, 0.00f, 0.50f, 0.60f); // Pink hover
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.00f, 1.00f, 1.00f, 1.00f); // Cyan active
    
    colors[ImGuiCol_Header]                 = ImVec4(0.18f, 0.10f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(1.00f, 0.00f, 0.50f, 0.40f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.00f, 1.00f, 1.00f, 0.80f);
    
    colors[ImGuiCol_Separator]              = ImVec4(1.00f, 0.00f, 0.50f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(1.00f, 0.00f, 0.50f, 0.80f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    
    colors[ImGuiCol_Tab]                    = ImVec4(0.10f, 0.07f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(1.00f, 0.00f, 0.50f, 0.50f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.00f, 1.00f, 1.00f, 0.40f); // Muted Cyan Tab
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.09f, 0.07f, 0.15f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.12f, 0.09f, 0.22f, 1.00f);
    
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.00f, 1.00f, 1.00f, 0.40f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.05f, 0.04f, 0.09f, 1.00f);

    editorTheme.text = ImVec4(0.95f, 0.95f, 0.00f, 1.00f);
    editorTheme.keyword = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    editorTheme.macro = ImVec4(1.00f, 0.00f, 0.50f, 1.00f);
    editorTheme.type = ImVec4(0.70f, 1.00f, 0.20f, 1.00f);
    editorTheme.string = ImVec4(1.00f, 0.40f, 0.00f, 1.00f);
    editorTheme.comment = ImVec4(0.50f, 0.35f, 0.65f, 1.00f);
    editorTheme.lineNumbers = ImVec4(0.50f, 0.35f, 0.65f, 1.00f);
}

void UIManager::applyMonokaiTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 4.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    // Monokai Palette
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.90f, 1.00f); // Off-white
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.45f, 0.45f, 0.45f, 1.00f); // Gray
    colors[ImGuiCol_WindowBg]               = ImVec4(0.14f, 0.14f, 0.14f, 1.00f); // Dark Charcoal
    colors[ImGuiCol_ChildBg]                = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.10f, 0.10f, 0.10f, 0.96f);
    colors[ImGuiCol_Border]                 = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    colors[ImGuiCol_FrameBg]                = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    
    colors[ImGuiCol_TitleBg]                = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    
    colors[ImGuiCol_CheckMark]              = ImVec4(0.65f, 0.89f, 0.18f, 1.00f); // Monokai Green
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.65f, 0.89f, 0.18f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]        = ImVec4(0.75f, 0.95f, 0.25f, 1.00f);
    
    colors[ImGuiCol_Button]                 = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.98f, 0.15f, 0.45f, 1.00f); // Monokai Pink Active
    
    colors[ImGuiCol_Header]                 = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.98f, 0.15f, 0.45f, 0.80f);
    
    colors[ImGuiCol_Separator]              = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.98f, 0.15f, 0.45f, 1.00f);
    
    colors[ImGuiCol_Tab]                    = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.98f, 0.15f, 0.45f, 0.50f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    editorTheme.text = ImVec4(0.95f, 0.95f, 0.90f, 1.00f);
    editorTheme.keyword = ImVec4(0.98f, 0.15f, 0.45f, 1.00f);
    editorTheme.macro = ImVec4(0.68f, 0.51f, 1.00f, 1.00f);
    editorTheme.type = ImVec4(0.40f, 0.85f, 0.93f, 1.00f);
    editorTheme.string = ImVec4(0.90f, 0.86f, 0.45f, 1.00f);
    editorTheme.comment = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    editorTheme.lineNumbers = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
}

void UIManager::applyGithubLightTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 4.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    // Github Light Palette
    colors[ImGuiCol_Text]                   = ImVec4(0.15f, 0.15f, 0.18f, 1.00f); // Dark Slate
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f); // Muted Gray
    colors[ImGuiCol_WindowBg]               = ImVec4(0.96f, 0.97f, 0.98f, 1.00f); // Light Off-white/Gray
    colors[ImGuiCol_ChildBg]                = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // Pure White
    colors[ImGuiCol_PopupBg]                = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    colors[ImGuiCol_Border]                 = ImVec4(0.85f, 0.85f, 0.85f, 1.00f); // Light Border
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    colors[ImGuiCol_FrameBg]                = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.88f, 0.89f, 0.90f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.84f, 0.85f, 0.86f, 1.00f);
    
    colors[ImGuiCol_TitleBg]                = ImVec4(0.94f, 0.95f, 0.96f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.90f, 0.92f, 0.94f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.96f, 0.97f, 0.98f, 1.00f);
    
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.94f, 0.95f, 0.96f, 1.00f);
    
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.96f, 0.97f, 0.98f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    
    colors[ImGuiCol_CheckMark]              = ImVec4(0.01f, 0.40f, 0.84f, 1.00f); // Github Blue
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.01f, 0.40f, 0.84f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]        = ImVec4(0.20f, 0.60f, 1.00f, 1.00f);
    
    colors[ImGuiCol_Button]                 = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.01f, 0.40f, 0.84f, 0.20f); // Muted Blue Hover
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.01f, 0.40f, 0.84f, 1.00f); // Blue Active
    
    colors[ImGuiCol_Header]                 = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.01f, 0.40f, 0.84f, 0.15f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.01f, 0.40f, 0.84f, 0.30f);
    
    colors[ImGuiCol_Separator]              = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.01f, 0.40f, 0.84f, 0.60f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.01f, 0.40f, 0.84f, 1.00f);
    
    colors[ImGuiCol_Tab]                    = ImVec4(0.94f, 0.95f, 0.96f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.90f, 0.92f, 0.94f, 1.00f);
    colors[ImGuiCol_TabActive]              = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // Active white tab
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.96f, 0.97f, 0.98f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.01f, 0.40f, 0.84f, 0.30f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.96f, 0.97f, 0.98f, 1.00f);

    editorTheme.text = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    editorTheme.keyword = ImVec4(0.85f, 0.15f, 0.25f, 1.00f); // Red
    editorTheme.macro = ImVec4(0.50f, 0.20f, 0.80f, 1.00f); // Purple
    editorTheme.type = ImVec4(0.01f, 0.40f, 0.84f, 1.00f); // Blue
    editorTheme.string = ImVec4(0.10f, 0.45f, 0.20f, 1.00f); // Green
    editorTheme.comment = ImVec4(0.60f, 0.60f, 0.60f, 1.00f); // Gray
    editorTheme.lineNumbers = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
}

void UIManager::saveLayout(const std::string& presetName) {
    ImGui::SaveIniSettingsToDisk((presetName + ".ini").c_str());
    FORGE_LOG_INFO("UI", "Saved layout preset: " + presetName);
}

void UIManager::loadLayout(const std::string& presetName) {
    ImGui::LoadIniSettingsFromDisk((presetName + ".ini").c_str());
    FORGE_LOG_INFO("UI", "Loaded layout preset: " + presetName);
}

void UIManager::setCustomCompileCommand(const std::string& ext, const std::string& cmd) {
    customCompileCommands[ext] = cmd;
    saveSettings();
}

std::string UIManager::getCustomCompileCommand(const std::string& ext) {
    auto it = customCompileCommands.find(ext);
    if (it != customCompileCommands.end()) {
        return it->second;
    }
    return "";
}

void UIManager::setCustomRunCommand(const std::string& ext, const std::string& cmd) {
    customRunCommands[ext] = cmd;
    saveSettings();
}

std::string UIManager::getCustomRunCommand(const std::string& ext) {
    auto it = customRunCommands.find(ext);
    if (it != customRunCommands.end()) {
        return it->second;
    }
    return "";
}

void UIManager::saveSettings() {
#if defined(_WIN32)
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) != 0) {
        std::string exeDir = std::string(exePath).substr(0, std::string(exePath).find_last_of("\\/"));
        std::string settingsPath = exeDir + "\\settings.txt";
        std::ofstream settingsFile(settingsPath);
        if (settingsFile.is_open()) {
            settingsFile << Workspace::getInstance().getProjectPath() << "\n";
            settingsFile << activeThemeName << "\n";

            // Save runner custom configurations
            for (const auto& pair : customCompileCommands) {
                const std::string& ext = pair.first;
                const std::string& compile = pair.second;
                std::string run = "";
                auto runIt = customRunCommands.find(ext);
                if (runIt != customRunCommands.end()) {
                    run = runIt->second;
                }
                settingsFile << "runner:" << ext << "|" << compile << "|" << run << "\n";
            }
            for (const auto& pair : customRunCommands) {
                const std::string& ext = pair.first;
                if (customCompileCommands.find(ext) == customCompileCommands.end()) {
                    settingsFile << "runner:" << ext << "||" << pair.second << "\n";
                }
            }

            settingsFile.close();
            FORGE_LOG_INFO("UI", "Saved configuration settings: Theme=" + activeThemeName);
        }
    }
#endif
}

void UIManager::loadSettings() {
#if defined(_WIN32)
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) != 0) {
        std::string exeDir = std::string(exePath).substr(0, std::string(exePath).find_last_of("\\/"));
        std::string settingsPath = exeDir + "\\settings.txt";
        std::ifstream settingsFile(settingsPath);
        if (settingsFile.is_open()) {
            std::string projectPath;
            std::getline(settingsFile, projectPath); // Line 1: folder
            
            std::string theme;
            if (std::getline(settingsFile, theme) && !theme.empty()) {
                activeThemeName = theme;
            } else {
                activeThemeName = "Slate Dark";
            }

            // Load runner custom configurations
            std::string line;
            while (std::getline(settingsFile, line)) {
                if (line.rfind("runner:", 0) == 0) {
                    std::string payload = line.substr(7);
                    size_t pipe1 = payload.find('|');
                    if (pipe1 != std::string::npos) {
                        std::string ext = payload.substr(0, pipe1);
                        std::string rest = payload.substr(pipe1 + 1);
                        size_t pipe2 = rest.find('|');
                        if (pipe2 != std::string::npos) {
                            std::string compile = rest.substr(0, pipe2);
                            std::string run = rest.substr(pipe2 + 1);
                            customCompileCommands[ext] = compile;
                            customRunCommands[ext] = run;
                        }
                    }
                }
            }

            settingsFile.close();
            themeApplied = false; // re-trigger theme apply in renderAll
            FORGE_LOG_INFO("UI", "Loaded configuration settings: Theme=" + activeThemeName);
        }
    }
#endif
}

} // namespace forge
