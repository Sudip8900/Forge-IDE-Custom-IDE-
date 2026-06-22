#include "UIManager.hpp"
#include "../core/Workspace.hpp"
#include "../utils/Logger.hpp"
#include "panels/EditorPanel.hpp"
#include "panels/TerminalPanel.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

#if defined(_WIN32)
#include <commdlg.h>
#include <shlobj.h>
#include <windows.h>
#endif

namespace forge {

static void
collectFilesRecursive(const WorkspaceFile &node,
                      std::vector<std::pair<std::string, std::string>> &files) {
  if (!node.is_directory) {
    files.push_back({node.name, node.path});
  } else {
    for (const auto &child : node.children) {
      collectFilesRecursive(child, files);
    }
  }
}

UIManager &UIManager::getInstance() {
  static UIManager instance;
  return instance;
}

void UIManager::init() { loadSettings(); }

void UIManager::shutdown() { panels.clear(); }

void UIManager::addPanel(std::shared_ptr<Panel> panel) {
  panels.push_back(panel);
}

void UIManager::renderAll(bool showMenuBar) {
  // Handle UI Scaling Shortcuts (Only Code Editor Font)
  ImGuiIO &io = ImGui::GetIO();
  if (io.KeyCtrl) {
    if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
      editorFontScale += 0.1f;
      if (editorFontScale > 3.0f)
        editorFontScale = 3.0f;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
      editorFontScale -= 0.1f;
      if (editorFontScale < 0.5f)
        editorFontScale = 0.5f;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_0)) {
      editorFontScale = 1.0f;
    }
  }

  if (!themeApplied) {
    applyThemeByName(activeThemeName);
    themeApplied = true;
  }

  // Retrieve panel pointers for interaction
  std::shared_ptr<Panel> explorerPanel;
  std::shared_ptr<Panel> terminalPanel;
  std::shared_ptr<Panel> outputPanel;
  std::shared_ptr<Panel> editorPanel;

  for (auto &p : panels) {
    std::string name = p->getName();
    if (name == "PROJECT")
      explorerPanel = p;
    else if (name == "Integrated Terminal")
      terminalPanel = p;
    else if (name == "Output Window")
      outputPanel = p;
    else if (name == "Code Editor")
      editorPanel = p;
  }

  // Handle Global Ctrl+S Save Shortcut
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
    if (editorPanel) {
      auto editor = std::dynamic_pointer_cast<EditorPanel>(editorPanel);
      if (editor) {
        editor->saveCurrentFile();
      }
    }
  }

  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImVec2 workPos = viewport->WorkPos;
  ImVec2 workSize = viewport->WorkSize;

  float topBarHeight = 40.0f;
  float bottomBarHeight = 25.0f;
  float activityBarWidth = 55.0f;

  // 1. TOP BAR
  ImGui::SetNextWindowPos(workPos);
  ImGui::SetNextWindowSize(ImVec2(workSize.x, topBarHeight));
  ImGui::SetNextWindowViewport(viewport->ID);
  ImGuiWindowFlags topFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        ImVec4(0.043f, 0.047f, 0.055f, 1.00f)); // Top Bar Color
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin("TopHeaderBar", nullptr, topFlags);
  ImGui::PopStyleVar(2);

  ImDrawList *topDrawList = ImGui::GetWindowDrawList();
  ImVec2 topPos = ImGui::GetWindowPos();

  // Draw bottom separator
  topDrawList->AddLine(
      ImVec2(topPos.x, topPos.y + topBarHeight - 1.0f),
      ImVec2(topPos.x + workSize.x, topPos.y + topBarHeight - 1.0f),
      ImColor(editorTheme.lineNumbers));

  // Top-left app title/logo
  ImGui::SetCursorScreenPos(
      ImVec2(topPos.x + 15.0f,
             topPos.y + (topBarHeight - ImGui::GetTextLineHeight()) * 0.5f));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.00f, 0.90f, 0.46f, 1.00f));
  ImGui::Text("code ZEN");
  ImGui::PopStyleColor();

  // Small file/window menus next to title
  float menuStart = 110.0f;
  ImGui::SetCursorScreenPos(
      ImVec2(topPos.x + menuStart, topPos.y + (topBarHeight - 20.0f) * 0.5f));

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.1f, 0.12f, 0.15f, 1.0f));

  if (ImGui::Button("File")) {
    ImGui::OpenPopup("FileMenuPopup");
  }
  ImGui::SameLine();
  if (ImGui::Button("Window")) {
    ImGui::OpenPopup("WindowMenuPopup");
  }

  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar();

  // Popups for menus
  if (ImGui::BeginPopup("FileMenuPopup")) {
    if (ImGui::MenuItem("New File...", "Ctrl+N")) {
#if defined(_WIN32)
      char filename[MAX_PATH] = "";
      OPENFILENAMEA ofn;
      ZeroMemory(&ofn, sizeof(ofn));
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = NULL;
      ofn.lpstrFilter = "C++ Source (*.cpp)\0*.cpp\0C++ Header "
                        "(*.h;*.hpp)\0*.h;*.hpp\0All Files (*.*)\0*.*\0";
      ofn.lpstrFile = filename;
      ofn.nMaxFile = MAX_PATH;
      ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
                  OFN_OVERWRITEPROMPT;
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
      ofn.lpstrFilter = "All Files (*.*)\0*.*\0C++ Files "
                        "(*.cpp;*.h;*.hpp)\0*.cpp;*.h;*.hpp\0";
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
      BROWSEINFOA bi = {0};
      bi.lpszTitle = "Select Project Folder";
      bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
      LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
      if (pidl != 0) {
        SHGetPathFromIDListA(pidl, path);
        IMalloc *imalloc = 0;
        if (SUCCEEDED(SHGetMalloc(&imalloc))) {
          imalloc->Free(pidl);
          imalloc->Release();
        }
        Workspace::getInstance().openProject(path);
      }
#endif
    }
    if (ImGui::MenuItem("Save", "Ctrl+S")) {
      if (editorPanel) {
        auto editor = std::dynamic_pointer_cast<EditorPanel>(editorPanel);
        if (editor) {
          editor->saveCurrentFile();
        }
      }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Exit", "Alt+F4")) {
      // Exit command will trigger on next frame
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopup("WindowMenuPopup")) {
    for (auto &panel : panels) {
      bool openVal = panel->isOpen();
      if (ImGui::MenuItem(panel->getName(), nullptr, &openVal)) {
        panel->setOpen(openVal);
      }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Reset Layout")) {
      resetLayout = true;
    }
    ImGui::EndPopup();
  }

  // Center Search Input box
  float searchWidth = 660.0f;
  float searchHeight = 24.0f;
  ImVec2 searchPos = ImVec2(topPos.x + (workSize.x - searchWidth) * 0.5f,
                            topPos.y + (topBarHeight - searchHeight) * 0.5f);

  topDrawList->AddRectFilled(
      searchPos, ImVec2(searchPos.x + searchWidth, searchPos.y + searchHeight),
      ImColor(16, 18, 22), 4.0f);

  ImVec2 mousePos = ImGui::GetMousePos();
  bool searchHovered =
      (mousePos.x >= searchPos.x && mousePos.x <= searchPos.x + searchWidth &&
       mousePos.y >= searchPos.y && mousePos.y <= searchPos.y + searchHeight);

  ImColor searchBorderColor =
      searchHovered ? ImColor(0, 230, 118) : ImColor(28, 30, 36);
  topDrawList->AddRect(
      searchPos, ImVec2(searchPos.x + searchWidth, searchPos.y + searchHeight),
      searchBorderColor, 4.0f, 0, 1.0f);

  ImVec2 searchIconCenter = ImVec2(searchPos.x + 12.0f, searchPos.y + 12.0f);
  topDrawList->AddCircle(
      ImVec2(searchIconCenter.x - 1.5f, searchIconCenter.y - 1.5f), 3.0f,
      ImColor(156, 163, 175), 12, 1.0f);
  topDrawList->AddLine(
      ImVec2(searchIconCenter.x + 0.5f, searchIconCenter.y + 0.5f),
      ImVec2(searchIconCenter.x + 4.5f, searchIconCenter.y + 4.5f),
      ImColor(156, 163, 175), 1.2f);

  ImGui::SetCursorScreenPos(ImVec2(searchPos.x + 24.0f, searchPos.y + 4.0f));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.43f, 0.48f, 1.00f));
  ImGui::Text("Search files, symbols...");
  ImGui::PopStyleColor();

  // Make the search box a clickable button
  ImGui::SetCursorScreenPos(searchPos);
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
  if (ImGui::Button("##searchBarBtn", ImVec2(searchWidth, searchHeight))) {
    ImGui::OpenPopup("GoToSearchPopup");
  }
  ImGui::PopStyleColor(3);

  // Dropdown Search Popup
  ImGui::SetNextWindowPos(
      ImVec2(searchPos.x, searchPos.y + searchHeight + 2.0f));
  ImGui::SetNextWindowSize(ImVec2(searchWidth, 250.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
  if (ImGui::BeginPopup("GoToSearchPopup",
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
    static char searchInputBuffer[256] = "";

    // Auto-focus input
    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
      searchInputBuffer[0] = '\0';
    }

    ImGui::PushItemWidth(-1);
    ImGui::InputText("##searchQuery", searchInputBuffer,
                     sizeof(searchInputBuffer));
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Traverse files in workspace
    WorkspaceFile root = Workspace::getInstance().getFileTree();
    std::vector<std::pair<std::string, std::string>> allFiles;
    for (const auto &child : root.children) {
      collectFilesRecursive(child, allFiles);
    }

    std::string query = searchInputBuffer;
    std::string queryLower = query;
    std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(),
                   ::tolower);

    std::vector<std::pair<std::string, std::string>> matches;
    std::string projectPath = Workspace::getInstance().getProjectPath();

    for (const auto &f : allFiles) {
      std::string relPath = f.second;
      if (relPath.rfind(projectPath, 0) == 0 &&
          projectPath.length() < relPath.length()) {
        relPath = relPath.substr(projectPath.length() + 1);
      }

      std::string relPathLower = relPath;
      std::transform(relPathLower.begin(), relPathLower.end(),
                     relPathLower.begin(), ::tolower);

      if (queryLower.empty() ||
          relPathLower.find(queryLower) != std::string::npos) {
        matches.push_back({relPath, f.second});
      }
    }

    ImGui::BeginChild("SearchMatchesList", ImVec2(0, 0), false);
    for (size_t i = 0; i < matches.size(); i++) {
      if (ImGui::Selectable(matches[i].first.c_str(), false)) {
        Workspace::getInstance().openDocument(matches[i].second);
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndChild();

    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();

  // Right action buttons (Only Play/Run button)
  float bx = topPos.x + workSize.x - 50.0f;
  float by = topPos.y + (topBarHeight - 30.0f) * 0.5f;
  ImGui::SetCursorScreenPos(ImVec2(bx, by));

  if (ImGui::InvisibleButton("##topActBtn_Run", ImVec2(30.0f, 30.0f))) {
    std::string activeDoc = Workspace::getInstance().getActiveDocument();
    if (!activeDoc.empty() && editorPanel) {
      auto editor = std::dynamic_pointer_cast<EditorPanel>(editorPanel);
      if (editor) {
        editor->runActiveFile(activeDoc);
      }
    }
  }

  bool actHovered = ImGui::IsItemHovered();
  ImColor actColor = actHovered ? ImColor(0, 230, 118) : ImColor(156, 163, 175);
  ImVec2 cb = ImVec2(bx + 15.0f, by + 15.0f);

  ImVec2 p1 = ImVec2(cb.x - 4.0f, cb.y - 6.0f);
  ImVec2 p2 = ImVec2(cb.x - 4.0f, cb.y + 6.0f);
  ImVec2 p3 = ImVec2(cb.x + 6.0f, cb.y);
  topDrawList->AddTriangleFilled(p1, p2, p3, actColor);

  ImGui::End();
  ImGui::PopStyleColor();

  // 2. ACTIVITY BAR (LEFT)
  ImGui::SetNextWindowPos(ImVec2(workPos.x, workPos.y + topBarHeight));
  ImGui::SetNextWindowSize(
      ImVec2(activityBarWidth, workSize.y - topBarHeight - bottomBarHeight));
  ImGui::SetNextWindowViewport(viewport->ID);
  ImGuiWindowFlags activityFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        ImVec4(0.086f, 0.09f, 0.106f, 1.00f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin("ActivityBar", nullptr, activityFlags);
  ImGui::PopStyleVar(2);

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 pos = ImGui::GetWindowPos();

  drawList->AddLine(ImVec2(pos.x + activityBarWidth - 1.0f, pos.y),
                    ImVec2(pos.x + activityBarWidth - 1.0f,
                           pos.y + workSize.y - topBarHeight - bottomBarHeight),
                    ImColor(editorTheme.lineNumbers));

  int activeIconIndex = -1;
  if (explorerPanel && explorerPanel->isOpen()) {
    activeIconIndex = 0;
  } else if (terminalPanel && terminalPanel->isOpen()) {
    activeIconIndex = 1;
  }

  for (int i = 0; i < 2; i++) {
    ImVec2 btnPos = ImVec2(pos.x, pos.y + i * 55.0f);
    ImGui::SetCursorScreenPos(btnPos);

    std::string btnId = "##actBtn_" + std::to_string(i);
    if (ImGui::InvisibleButton(btnId.c_str(), ImVec2(55.0f, 55.0f))) {
      if (i == 0) {
        if (explorerPanel)
          explorerPanel->setOpen(!explorerPanel->isOpen());
      } else if (i == 1) {
        if (terminalPanel) {
          terminalPanel->setOpen(!terminalPanel->isOpen());
        }
      }
    }

    bool isHovered = ImGui::IsItemHovered();
    bool isActive = (i == activeIconIndex);

    ImColor iconColor = isActive ? ImColor(0, 230, 118)
                                 : (isHovered ? ImColor(255, 255, 255)
                                              : ImColor(107, 114, 128));
    ImVec2 cx = ImVec2(btnPos.x + 27.5f, btnPos.y + 27.5f);

    if (isActive) {
      drawList->AddRectFilled(ImVec2(btnPos.x, btnPos.y + 12.0f),
                              ImVec2(btnPos.x + 3.0f, btnPos.y + 43.0f),
                              ImColor(0, 230, 118));
    }

    if (i == 0) {
      drawList->AddRect(ImVec2(cx.x - 5.0f, cx.y - 7.0f),
                        ImVec2(cx.x + 7.0f, cx.y + 9.0f), iconColor, 0.0f, 0,
                        1.2f);
      drawList->AddRectFilled(ImVec2(cx.x - 9.0f, cx.y - 3.0f),
                              ImVec2(cx.x + 3.0f, cx.y + 13.0f),
                              ImColor(14, 15, 17));
      drawList->AddRect(ImVec2(cx.x - 9.0f, cx.y - 3.0f),
                        ImVec2(cx.x + 3.0f, cx.y + 13.0f), iconColor, 0.0f, 0,
                        1.2f);
    } else if (i == 1) {
      // Draw Terminal Icon: ">_" prompt
      // Greater-than sign:
      drawList->AddLine(ImVec2(cx.x - 6.0f, cx.y - 5.0f),
                        ImVec2(cx.x - 1.0f, cx.y), iconColor, 1.8f);
      drawList->AddLine(ImVec2(cx.x - 1.0f, cx.y),
                        ImVec2(cx.x - 6.0f, cx.y + 5.0f), iconColor, 1.8f);
      // Underscore cursor:
      drawList->AddLine(ImVec2(cx.x + 1.0f, cx.y + 5.0f),
                        ImVec2(cx.x + 7.0f, cx.y + 5.0f), iconColor, 1.8f);
    }
  }

  ImGui::End();
  ImGui::PopStyleColor();

  // 3. STATUS BAR (BOTTOM)
  ImGui::SetNextWindowPos(
      ImVec2(workPos.x, workPos.y + workSize.y - bottomBarHeight));
  ImGui::SetNextWindowSize(ImVec2(workSize.x, bottomBarHeight));
  ImGui::SetNextWindowViewport(viewport->ID);
  ImGuiWindowFlags bottomFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        ImVec4(0.043f, 0.047f, 0.055f, 1.00f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin("StatusBottomBar", nullptr, bottomFlags);
  ImGui::PopStyleVar(2);

  ImDrawList *botDrawList = ImGui::GetWindowDrawList();
  ImVec2 botPos = ImGui::GetWindowPos();

  botDrawList->AddLine(ImVec2(botPos.x, botPos.y),
                       ImVec2(botPos.x + workSize.x, botPos.y),
                       ImColor(editorTheme.lineNumbers));

  // Left side: branch info & sync status
  ImVec2 gitIconPos =
      ImVec2(botPos.x + 15.0f, botPos.y + bottomBarHeight * 0.5f);
  botDrawList->AddLine(ImVec2(gitIconPos.x - 2.0f, gitIconPos.y - 5.0f),
                       ImVec2(gitIconPos.x - 2.0f, gitIconPos.y + 5.0f),
                       ImColor(156, 163, 175), 1.0f);
  botDrawList->AddLine(ImVec2(gitIconPos.x - 2.0f, gitIconPos.y),
                       ImVec2(gitIconPos.x + 2.0f, gitIconPos.y - 5.0f),
                       ImColor(156, 163, 175), 1.0f);
  botDrawList->AddCircleFilled(ImVec2(gitIconPos.x - 2.0f, gitIconPos.y - 5.0f),
                               1.5f, ImColor(156, 163, 175));
  botDrawList->AddCircleFilled(ImVec2(gitIconPos.x - 2.0f, gitIconPos.y + 5.0f),
                               1.5f, ImColor(156, 163, 175));
  botDrawList->AddCircleFilled(ImVec2(gitIconPos.x + 2.0f, gitIconPos.y - 5.0f),
                               1.5f, ImColor(156, 163, 175));

  ImGui::SetCursorScreenPos(
      ImVec2(botPos.x + 27.0f,
             botPos.y + (bottomBarHeight - ImGui::GetTextLineHeight()) * 0.5f));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.61f, 0.64f, 0.70f, 1.00f));
  ImGui::Text("main");
  ImGui::PopStyleColor();

  ImVec2 syncCenter =
      ImVec2(botPos.x + 68.0f, botPos.y + bottomBarHeight * 0.5f);
  botDrawList->AddCircle(syncCenter, 3.5f, ImColor(156, 163, 175), 12, 1.0f);
  botDrawList->AddTriangleFilled(
      ImVec2(syncCenter.x + 3.0f, syncCenter.y - 2.0f),
      ImVec2(syncCenter.x + 5.0f, syncCenter.y),
      ImVec2(syncCenter.x + 2.0f, syncCenter.y), ImColor(156, 163, 175));

  // Right side: Ln/Col info & lightning badge
  std::string activeDoc = Workspace::getInstance().getActiveDocument();
  std::string langName = "Plain Text";
  if (!activeDoc.empty()) {
    fs::path p(activeDoc);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".cpp" || ext == ".h" || ext == ".hpp")
      langName = "C++";
    else if (ext == ".py")
      langName = "Python";
    else if (ext == ".js")
      langName = "JavaScript";
    else if (ext == ".html" || ext == ".htm")
      langName = "HTML";
    else if (ext == ".go")
      langName = "Go";
    else if (ext == ".rs")
      langName = "Rust";
    else if (ext == ".java")
      langName = "Java";
  }

  char statusText[128];
  sprintf_s(statusText, "Ln %d, Col %d       UTF-8       <> %s", cursorLine,
            cursorCol, langName.c_str());

  float statusTextWidth = ImGui::CalcTextSize(statusText).x;
  ImGui::SetCursorScreenPos(
      ImVec2(botPos.x + workSize.x - statusTextWidth - 50.0f,
             botPos.y + (bottomBarHeight - ImGui::GetTextLineHeight()) * 0.5f));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.61f, 0.64f, 0.70f, 1.00f));
  ImGui::Text("%s", statusText);
  ImGui::PopStyleColor();

  float badgeWidth = 26.0f;
  float badgeHeight = bottomBarHeight;
  ImVec2 badgePos = ImVec2(botPos.x + workSize.x - badgeWidth, botPos.y);

  botDrawList->AddRectFilled(
      badgePos, ImVec2(badgePos.x + badgeWidth, badgePos.y + badgeHeight),
      ImColor(0, 230, 118));

  ImVec2 centerBadge =
      ImVec2(badgePos.x + badgeWidth * 0.5f, badgePos.y + badgeHeight * 0.5f);
  botDrawList->AddTriangleFilled(
      ImVec2(centerBadge.x + 2.0f, centerBadge.y - 7.0f),
      ImVec2(centerBadge.x - 3.0f, centerBadge.y + 1.0f),
      ImVec2(centerBadge.x + 1.0f, centerBadge.y + 1.0f), ImColor(0, 0, 0));
  botDrawList->AddTriangleFilled(
      ImVec2(centerBadge.x - 1.0f, centerBadge.y - 1.0f),
      ImVec2(centerBadge.x + 3.0f, centerBadge.y - 1.0f),
      ImVec2(centerBadge.x - 2.0f, centerBadge.y + 7.0f), ImColor(0, 0, 0));

  ImGui::End();
  ImGui::PopStyleColor();

  // 4. MAIN WORKSPACE DOCKSPACE
  createMainDockSpace();

  // Render each panel inside dockspace
  for (auto &panel : panels) {
    if (panel->isOpen()) {
      panel->render();
    }
  }
}

void UIManager::createMainDockSpace() {
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  float topBarHeight = 40.0f;
  float bottomBarHeight = 25.0f;
  float activityBarWidth = 55.0f;

  ImVec2 dockPos = ImVec2(viewport->WorkPos.x + activityBarWidth,
                          viewport->WorkPos.y + topBarHeight);
  ImVec2 dockSize =
      ImVec2(viewport->WorkSize.x - activityBarWidth,
             viewport->WorkSize.y - topBarHeight - bottomBarHeight);

  ImGui::SetNextWindowPos(dockPos);
  ImGui::SetNextWindowSize(dockSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
  window_flags |=
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  bool open = true;
  ImGui::Begin("codeZENDockSpaceWindow", &open, window_flags);
  ImGui::PopStyleVar(3);

  // Dockspace
  ImGuiIO &io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
    ImGuiID dockspace_id = ImGui::GetID("codeZENDockSpace");

    static bool reset_requested = false;
    if (resetLayout) {
      reset_requested = true;
      resetLayout = false;
    }

    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr || reset_requested) {
      reset_requested = false;
      ImGui::DockBuilderRemoveNode(dockspace_id);
      ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dockspace_id, dockSize);

      ImGuiID dock_main_id = dockspace_id;
      ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(
          dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
      ImGuiID dock_down_id = ImGui::DockBuilderSplitNode(
          dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

      ImGui::DockBuilderDockWindow("PROJECT", dock_left_id);
      ImGui::DockBuilderDockWindow("Code Editor", dock_main_id);
      ImGui::DockBuilderDockWindow("Integrated Terminal", dock_down_id);
      ImGui::DockBuilderDockWindow("Output Window", dock_down_id);

      ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
  }

  ImGui::End();
}

void UIManager::applyThemeByName(const std::string &name) {
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
  ImGuiStyle &style = ImGui::GetStyle();
  ImVec4 *colors = style.Colors;

  style.WindowRounding = 0.0f;
  style.ChildRounding = 0.0f;
  style.FrameRounding = 3.0f;
  style.GrabRounding = 3.0f;
  style.TabRounding = 0.0f;

  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.PopupBorderSize = 1.0f;

  style.ItemSpacing = ImVec2(8.0f, 6.0f);
  style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
  style.WindowPadding = ImVec2(8.0f, 8.0f);

  // Deep modern dark-green accented theme matching the screenshot
  ImVec4 bgDark = ImVec4(0.043f, 0.047f, 0.055f, 1.00f);  // #0b0c0e
  ImVec4 panelBg = ImVec4(0.075f, 0.078f, 0.09f, 1.00f);  // #13141a
  ImVec4 borderDark = ImVec4(0.11f, 0.12f, 0.14f, 1.00f); // #1c1e24
  ImVec4 accentGreen =
      ImVec4(0.00f, 0.90f, 0.46f, 1.00f); // #00E676 (Mint green)
  ImVec4 accentGreenTrans = ImVec4(0.00f, 0.90f, 0.46f, 0.25f);
  ImVec4 textWhite = ImVec4(0.85f, 0.87f, 0.90f, 1.00f);
  ImVec4 textDisabled = ImVec4(0.40f, 0.43f, 0.48f, 1.00f);

  colors[ImGuiCol_Text] = textWhite;
  colors[ImGuiCol_TextDisabled] = textDisabled;
  colors[ImGuiCol_WindowBg] = bgDark;
  colors[ImGuiCol_ChildBg] = panelBg;
  colors[ImGuiCol_PopupBg] = panelBg;
  colors[ImGuiCol_Border] = borderDark;
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  colors[ImGuiCol_FrameBg] = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.16f, 0.20f, 1.00f);

  colors[ImGuiCol_TitleBg] = panelBg;
  colors[ImGuiCol_TitleBgActive] = panelBg;
  colors[ImGuiCol_TitleBgCollapsed] = bgDark;

  colors[ImGuiCol_MenuBarBg] = panelBg;

  colors[ImGuiCol_ScrollbarBg] = bgDark;
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.22f, 0.25f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25f, 0.28f, 0.32f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = accentGreen;

  colors[ImGuiCol_CheckMark] = accentGreen;
  colors[ImGuiCol_SliderGrab] = accentGreen;
  colors[ImGuiCol_SliderGrabActive] = accentGreen;

  colors[ImGuiCol_Button] = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.17f, 0.18f, 0.22f, 1.00f);
  colors[ImGuiCol_ButtonActive] = accentGreen;

  colors[ImGuiCol_Header] = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.15f, 0.16f, 0.20f, 1.00f);
  colors[ImGuiCol_HeaderActive] = accentGreenTrans;

  colors[ImGuiCol_Separator] = borderDark;
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.20f, 0.22f, 0.25f, 1.00f);
  colors[ImGuiCol_SeparatorActive] = accentGreen;

  colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
  colors[ImGuiCol_TabActive] = panelBg;
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.06f, 0.07f, 0.08f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = panelBg;

  colors[ImGuiCol_DockingPreview] = accentGreenTrans;
  colors[ImGuiCol_DockingEmptyBg] = bgDark;

  // Syntax highlighting colors
  editorTheme.text = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  editorTheme.keyword = accentGreen;
  editorTheme.macro = accentGreen;
  editorTheme.type = accentGreen;
  editorTheme.string = ImVec4(0.90f, 0.90f, 0.82f, 1.00f);
  editorTheme.comment = ImVec4(0.40f, 0.43f, 0.45f, 1.00f);
  editorTheme.lineNumbers = ImVec4(0.30f, 0.33f, 0.35f, 1.00f);
}

void UIManager::applyCyberpunkTheme() {
  ImGuiStyle &style = ImGui::GetStyle();
  ImVec4 *colors = style.Colors;

  style.WindowRounding = 0.0f;
  style.ChildRounding = 0.0f;
  style.FrameRounding = 0.0f;
  style.GrabRounding = 0.0f;
  style.TabRounding = 0.0f;

  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;
  style.PopupBorderSize = 1.0f;

  // Cyberpunk Neon colors
  colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.00f, 1.00f); // Neon Yellow
  colors[ImGuiCol_TextDisabled] =
      ImVec4(0.50f, 0.35f, 0.65f, 1.00f); // Dim Purple
  colors[ImGuiCol_WindowBg] =
      ImVec4(0.05f, 0.04f, 0.09f, 1.00f); // Deep Purple/Black
  colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.07f, 0.15f, 1.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.07f, 0.15f, 0.96f);
  colors[ImGuiCol_Border] = ImVec4(1.00f, 0.00f, 0.50f, 0.60f); // Neon Pink
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.09f, 0.22f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.14f, 0.33f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.19f, 0.44f, 1.00f);

  colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.07f, 0.15f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.08f, 0.25f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.04f, 0.09f, 1.00f);

  colors[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.07f, 0.15f, 1.00f);

  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.04f, 0.09f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] =
      ImVec4(1.00f, 0.00f, 0.50f, 0.40f); // Pink Grab
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.00f, 0.00f, 0.50f, 0.70f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.00f, 0.00f, 0.50f, 1.00f);

  colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f); // Neon Cyan
  colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 1.00f, 1.00f, 1.00f);

  colors[ImGuiCol_Button] = ImVec4(0.15f, 0.08f, 0.25f, 1.00f);
  colors[ImGuiCol_ButtonHovered] =
      ImVec4(1.00f, 0.00f, 0.50f, 0.60f); // Pink hover
  colors[ImGuiCol_ButtonActive] =
      ImVec4(0.00f, 1.00f, 1.00f, 1.00f); // Cyan active

  colors[ImGuiCol_Header] = ImVec4(0.18f, 0.10f, 0.30f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(1.00f, 0.00f, 0.50f, 0.40f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.80f);

  colors[ImGuiCol_Separator] = ImVec4(1.00f, 0.00f, 0.50f, 0.60f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(1.00f, 0.00f, 0.50f, 0.80f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);

  colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.07f, 0.18f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(1.00f, 0.00f, 0.50f, 0.50f);
  colors[ImGuiCol_TabActive] =
      ImVec4(0.00f, 1.00f, 1.00f, 0.40f); // Muted Cyan Tab
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.09f, 0.07f, 0.15f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.12f, 0.09f, 0.22f, 1.00f);

  colors[ImGuiCol_DockingPreview] = ImVec4(0.00f, 1.00f, 1.00f, 0.40f);
  colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.05f, 0.04f, 0.09f, 1.00f);

  editorTheme.text = ImVec4(0.95f, 0.95f, 0.00f, 1.00f);
  editorTheme.keyword = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
  editorTheme.macro = ImVec4(1.00f, 0.00f, 0.50f, 1.00f);
  editorTheme.type = ImVec4(0.70f, 1.00f, 0.20f, 1.00f);
  editorTheme.string = ImVec4(1.00f, 0.40f, 0.00f, 1.00f);
  editorTheme.comment = ImVec4(0.50f, 0.35f, 0.65f, 1.00f);
  editorTheme.lineNumbers = ImVec4(0.50f, 0.35f, 0.65f, 1.00f);
}

void UIManager::applyMonokaiTheme() {
  ImGuiStyle &style = ImGui::GetStyle();
  ImVec4 *colors = style.Colors;

  style.WindowRounding = 4.0f;
  style.ChildRounding = 3.0f;
  style.FrameRounding = 3.0f;
  style.GrabRounding = 3.0f;
  style.TabRounding = 4.0f;

  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.PopupBorderSize = 1.0f;

  // Monokai Palette
  colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.90f, 1.00f); // Off-white
  colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f); // Gray
  colors[ImGuiCol_WindowBg] =
      ImVec4(0.14f, 0.14f, 0.14f, 1.00f); // Dark Charcoal
  colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.96f);
  colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);

  colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);

  colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

  colors[ImGuiCol_CheckMark] =
      ImVec4(0.65f, 0.89f, 0.18f, 1.00f); // Monokai Green
  colors[ImGuiCol_SliderGrab] = ImVec4(0.65f, 0.89f, 0.18f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.75f, 0.95f, 0.25f, 1.00f);

  colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_ButtonActive] =
      ImVec4(0.98f, 0.15f, 0.45f, 1.00f); // Monokai Pink Active

  colors[ImGuiCol_Header] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.98f, 0.15f, 0.45f, 0.80f);

  colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.98f, 0.15f, 0.45f, 1.00f);

  colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
  colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);

  colors[ImGuiCol_DockingPreview] = ImVec4(0.98f, 0.15f, 0.45f, 0.50f);
  colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

  editorTheme.text = ImVec4(0.95f, 0.95f, 0.90f, 1.00f);
  editorTheme.keyword = ImVec4(0.98f, 0.15f, 0.45f, 1.00f);
  editorTheme.macro = ImVec4(0.68f, 0.51f, 1.00f, 1.00f);
  editorTheme.type = ImVec4(0.40f, 0.85f, 0.93f, 1.00f);
  editorTheme.string = ImVec4(0.90f, 0.86f, 0.45f, 1.00f);
  editorTheme.comment = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
  editorTheme.lineNumbers = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
}

void UIManager::applyGithubLightTheme() {
  ImGuiStyle &style = ImGui::GetStyle();
  ImVec4 *colors = style.Colors;

  style.WindowRounding = 4.0f;
  style.ChildRounding = 3.0f;
  style.FrameRounding = 3.0f;
  style.GrabRounding = 3.0f;
  style.TabRounding = 4.0f;

  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;
  style.PopupBorderSize = 1.0f;

  // Github Light Palette
  colors[ImGuiCol_Text] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f); // Dark Slate
  colors[ImGuiCol_TextDisabled] =
      ImVec4(0.60f, 0.60f, 0.60f, 1.00f); // Muted Gray
  colors[ImGuiCol_WindowBg] =
      ImVec4(0.96f, 0.97f, 0.98f, 1.00f); // Light Off-white/Gray
  colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // Pure White
  colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
  colors[ImGuiCol_Border] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f); // Light Border
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  colors[ImGuiCol_FrameBg] = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.88f, 0.89f, 0.90f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.84f, 0.85f, 0.86f, 1.00f);

  colors[ImGuiCol_TitleBg] = ImVec4(0.94f, 0.95f, 0.96f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.90f, 0.92f, 0.94f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.96f, 0.97f, 0.98f, 1.00f);

  colors[ImGuiCol_MenuBarBg] = ImVec4(0.94f, 0.95f, 0.96f, 1.00f);

  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.96f, 0.97f, 0.98f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);

  colors[ImGuiCol_CheckMark] =
      ImVec4(0.01f, 0.40f, 0.84f, 1.00f); // Github Blue
  colors[ImGuiCol_SliderGrab] = ImVec4(0.01f, 0.40f, 0.84f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.20f, 0.60f, 1.00f, 1.00f);

  colors[ImGuiCol_Button] = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
  colors[ImGuiCol_ButtonHovered] =
      ImVec4(0.01f, 0.40f, 0.84f, 0.20f); // Muted Blue Hover
  colors[ImGuiCol_ButtonActive] =
      ImVec4(0.01f, 0.40f, 0.84f, 1.00f); // Blue Active

  colors[ImGuiCol_Header] = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.01f, 0.40f, 0.84f, 0.15f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.01f, 0.40f, 0.84f, 0.30f);

  colors[ImGuiCol_Separator] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.01f, 0.40f, 0.84f, 0.60f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.01f, 0.40f, 0.84f, 1.00f);

  colors[ImGuiCol_Tab] = ImVec4(0.94f, 0.95f, 0.96f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.90f, 0.92f, 0.94f, 1.00f);
  colors[ImGuiCol_TabActive] =
      ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // Active white tab
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.96f, 0.97f, 0.98f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

  colors[ImGuiCol_DockingPreview] = ImVec4(0.01f, 0.40f, 0.84f, 0.30f);
  colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.96f, 0.97f, 0.98f, 1.00f);

  editorTheme.text = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
  editorTheme.keyword = ImVec4(0.85f, 0.15f, 0.25f, 1.00f); // Red
  editorTheme.macro = ImVec4(0.50f, 0.20f, 0.80f, 1.00f);   // Purple
  editorTheme.type = ImVec4(0.01f, 0.40f, 0.84f, 1.00f);    // Blue
  editorTheme.string = ImVec4(0.10f, 0.45f, 0.20f, 1.00f);  // Green
  editorTheme.comment = ImVec4(0.60f, 0.60f, 0.60f, 1.00f); // Gray
  editorTheme.lineNumbers = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
}

void UIManager::saveLayout(const std::string &presetName) {
  ImGui::SaveIniSettingsToDisk((presetName + ".ini").c_str());
  FORGE_LOG_INFO("UI", "Saved layout preset: " + presetName);
}

void UIManager::loadLayout(const std::string &presetName) {
  ImGui::LoadIniSettingsFromDisk((presetName + ".ini").c_str());
  FORGE_LOG_INFO("UI", "Loaded layout preset: " + presetName);
}

void UIManager::setCustomCompileCommand(const std::string &ext,
                                        const std::string &cmd) {
  customCompileCommands[ext] = cmd;
  saveSettings();
}

std::string UIManager::getCustomCompileCommand(const std::string &ext) {
  auto it = customCompileCommands.find(ext);
  if (it != customCompileCommands.end()) {
    return it->second;
  }
  return "";
}

void UIManager::setCustomRunCommand(const std::string &ext,
                                    const std::string &cmd) {
  customRunCommands[ext] = cmd;
  saveSettings();
}

std::string UIManager::getCustomRunCommand(const std::string &ext) {
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
    std::string exeDir = std::string(exePath).substr(
        0, std::string(exePath).find_last_of("\\/"));
    std::string settingsPath = exeDir + "\\settings.txt";
    std::ofstream settingsFile(settingsPath);
    if (settingsFile.is_open()) {
      settingsFile << Workspace::getInstance().getProjectPath() << "\n";
      settingsFile << activeThemeName << "\n";
      settingsFile << "layoutVersion:1\n";

      // Save runner custom configurations
      for (const auto &pair : customCompileCommands) {
        const std::string &ext = pair.first;
        const std::string &compile = pair.second;
        std::string run = "";
        auto runIt = customRunCommands.find(ext);
        if (runIt != customRunCommands.end()) {
          run = runIt->second;
        }
        settingsFile << "runner:" << ext << "|" << compile << "|" << run
                     << "\n";
      }
      for (const auto &pair : customRunCommands) {
        const std::string &ext = pair.first;
        if (customCompileCommands.find(ext) == customCompileCommands.end()) {
          settingsFile << "runner:" << ext << "||" << pair.second << "\n";
        }
      }

      settingsFile.close();
      FORGE_LOG_INFO("UI",
                     "Saved configuration settings: Theme=" + activeThemeName);
    }
  }
#endif
}

void UIManager::loadSettings() {
#if defined(_WIN32)
  char exePath[MAX_PATH];
  if (GetModuleFileNameA(NULL, exePath, MAX_PATH) != 0) {
    std::string exeDir = std::string(exePath).substr(
        0, std::string(exePath).find_last_of("\\/"));
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

      // Load runner custom configurations and search for version
      std::string line;
      bool versionFound = false;
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
        } else if (line == "layoutVersion:1") {
          versionFound = true;
        }
      }

      if (!versionFound) {
        resetLayout = true;
      }

      settingsFile.close();
      themeApplied = false; // re-trigger theme apply in renderAll
      FORGE_LOG_INFO("UI",
                     "Loaded configuration settings: Theme=" + activeThemeName);
    }
  }
#endif
}

} // namespace forge
