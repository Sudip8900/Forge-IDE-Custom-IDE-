#include "TerminalPanel.hpp"
#include "../terminal/UnrealGitHelper.hpp"
#include "../../core/Workspace.hpp"
#include "../UIManager.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace forge {

// Helper: Convert UTF-32 character to UTF-8 string
static std::string utf32_to_utf8(char32_t cp) {
    std::string result;
    if (cp < 0x80) {
        result.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        result.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        result.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x200000) {
        result.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return result;
}

TerminalPanel::TerminalPanel() {
    instance = this;
    open = false;

    // Load profiles
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir = std::string(exePath).substr(0, std::string(exePath).find_last_of("\\/"));
    TerminalProfileManager::getInstance().loadProfiles(exeDir + "\\terminal_profiles.json");
    selectedProfileName = TerminalProfileManager::getInstance().defaultProfile;

    loadLayoutState();

    // If no tabs were restored, create a default tab
    if (tabs.empty()) {
        createTerminal("Terminal 1", selectedProfileName);
    }
}

TerminalPanel::~TerminalPanel() {
    saveLayoutState();
    tabs.clear();
    instance = nullptr;
}

std::shared_ptr<TerminalSession> TerminalPanel::createTerminal(const std::string& customName, 
                                                              const std::string& profileName, 
                                                              const std::string& customWorkingDir) {
    std::string shellName = profileName.empty() ? selectedProfileName : profileName;
    
    // Find profile
    TerminalProfile profile;
    bool found = false;
    for (const auto& p : TerminalProfileManager::getInstance().profiles) {
        if (p.name == shellName) {
            profile = p;
            found = true;
            break;
        }
    }
    if (!found && !TerminalProfileManager::getInstance().profiles.empty()) {
        profile = TerminalProfileManager::getInstance().profiles[0];
        shellName = profile.name;
    }

    // Determine working directory
    std::string dir = customWorkingDir;
    if (dir.empty()) {
        dir = Workspace::getInstance().isProjectOpen() ? 
              Workspace::getInstance().getProjectPath() : 
              std::filesystem::current_path().string();
    }

    std::wstring wShellPath(profile.path.begin(), profile.path.end());
    std::wstring wArgs(profile.args.begin(), profile.args.end());
    std::wstring wDir(dir.begin(), dir.end());

    std::string termName = customName.empty() ? (shellName + " (" + std::to_string(tabs.size() + 1) + ")") : customName;

    // Create session
    auto session = std::make_shared<TerminalSession>(termName, wShellPath, wArgs, wDir, envVariables);
    if (!session->start(80, 24)) {
        // Fall back to cmd.exe if specific profile fails
        std::wstring fallbackShell = L"cmd.exe";
        session = std::make_shared<TerminalSession>(termName, fallbackShell, L"", wDir, envVariables);
        session->start(80, 24);
    }

    // Add to split node/tab
    auto node = std::make_shared<TerminalSplitNode>();
    node->session = session;
    node->isSplit = false;

    auto tab = std::make_shared<TerminalTab>();
    tab->name = termName;
    tab->rootNode = node;

    tabs.push_back(tab);
    activeTabIndex = static_cast<int>(tabs.size()) - 1;
    focusedSession = session;
    focusedNode = node;

    return session;
}

void TerminalPanel::createSplit(std::shared_ptr<TerminalSplitNode> node, bool vertical) {
    if (!node || node->isSplit) return;

    // Duplicate current session parameter to split
    std::wstring wShell = node->session->getShellPath();
    std::wstring wArgs = node->session->getArguments();
    std::wstring wDir = node->session->getWorkingDir();

    std::string name = node->session->getName() + " (Split)";

    auto newSession = std::make_shared<TerminalSession>(name, wShell, wArgs, wDir, envVariables);
    newSession->start(node->session->getEmulator().getCols(), node->session->getEmulator().getRows());

    auto childA = std::make_shared<TerminalSplitNode>();
    childA->session = node->session;
    childA->isSplit = false;

    auto childB = std::make_shared<TerminalSplitNode>();
    childB->session = newSession;
    childB->isSplit = false;

    node->session = nullptr;
    node->isSplit = true;
    node->splitVertical = vertical;
    node->splitRatio = 0.5f;
    node->childA = childA;
    node->childB = childB;

    focusedSession = newSession;
    focusedNode = childB;
}

void TerminalPanel::runCommand(const std::string& cmdLine, const std::string& workingDir) {
    std::lock_guard<std::mutex> lock(deferredCommandsMutex);
    deferredCommands.push_back({cmdLine, workingDir});
}

void TerminalPanel::executeCommandInternal(const std::string& cmdLine, const std::string& workingDir) {
    if (!focusedSession || !focusedSession->isAlive()) {
        createTerminal("", selectedProfileName, workingDir);
    }

    if (focusedSession && focusedSession->isAlive()) {
        if (!workingDir.empty()) {
            std::wstring shellPath = focusedSession->getShellPath();
            std::string shellLower(shellPath.begin(), shellPath.end());
            std::transform(shellLower.begin(), shellLower.end(), shellLower.begin(), ::tolower);
            
            bool isPowerShell = (shellLower.find("powershell") != std::string::npos || 
                                 shellLower.find("pwsh") != std::string::npos);
            
            std::string cdCmd = "cd";
            std::string separator = " && ";
            std::string runPrefix = "";
            
            if (isPowerShell) {
                separator = " ; ";
                if (!cmdLine.empty() && cmdLine[0] == '"') {
                    runPrefix = "& ";
                }
            } else {
                cdCmd += " /d"; // Use /d for Cmd
            }
            
            std::string command = cdCmd + " \"" + workingDir + "\"" + separator + runPrefix + cmdLine;
            focusedSession->writeInput(command + "\r\n");
        } else {
            focusedSession->writeInput(cmdLine + "\r\n");
        }
    }
}

void TerminalPanel::killTerminal(std::shared_ptr<TerminalSession> session) {
    if (!session) return;
    session->kill();

    // Remove from split tree or tab list
    for (auto it = tabs.begin(); it != tabs.end(); ++it) {
        auto tab = *it;
        
        // Lambda to check and prune leaf session in split tree
        auto pruneSession = [&](auto& self, std::shared_ptr<TerminalSplitNode> node) -> bool {
            if (!node->isSplit) {
                return (node->session == session);
            }
            
            if (self(self, node->childA)) {
                // Child A matches, replace parent with Child B
                node->isSplit = node->childB->isSplit;
                node->splitVertical = node->childB->splitVertical;
                node->splitRatio = node->childB->splitRatio;
                node->session = node->childB->session;
                node->childA = node->childB->childA;
                node->childB = node->childB->childB;
                return false;
            }
            if (self(self, node->childB)) {
                // Child B matches, replace parent with Child A
                node->isSplit = node->childA->isSplit;
                node->splitVertical = node->childA->splitVertical;
                node->splitRatio = node->childA->splitRatio;
                node->session = node->childA->session;
                node->childB = node->childA->childB;
                node->childA = node->childA->childA;
                return false;
            }
            return false;
        };

        if (tab->rootNode->isSplit) {
            pruneSession(pruneSession, tab->rootNode);
        } else if (tab->rootNode->session == session) {
            // Remove whole tab
            tabs.erase(it);
            if (tabs.empty()) {
                focusedSession = nullptr;
                focusedNode = nullptr;
                activeTabIndex = 0;
            } else {
                activeTabIndex = std::clamp(activeTabIndex, 0, static_cast<int>(tabs.size()) - 1);
                std::vector<std::shared_ptr<TerminalSession>> activeSessions;
                tabs[activeTabIndex]->rootNode->getSessions(activeSessions);
                if (!activeSessions.empty()) {
                    focusedSession = activeSessions[0];
                }
            }
            return;
        }
    }
}

void TerminalPanel::renameTerminal(std::shared_ptr<TerminalSession> session, const std::string& newName) {
    if (!session) return;
    session->setName(newName);
    
    // Sync tab name if it's the root node
    for (auto& tab : tabs) {
        if (!tab->rootNode->isSplit && tab->rootNode->session == session) {
            tab->name = newName;
        }
    }
}

void TerminalPanel::render() {
    // Process deferred commands on the main UI thread
    std::vector<DeferCommand> toRun;
    {
        std::lock_guard<std::mutex> lock(deferredCommandsMutex);
        toRun = std::move(deferredCommands);
        deferredCommands.clear();
    }
    for (const auto& cmd : toRun) {
        open = true;
        executeCommandInternal(cmd.cmdLine, cmd.workingDir);
    }

    ImGui::SetNextWindowSize(ImVec2(600, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(getName(), &open, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    // Clean up dead tabs/sessions
    for (size_t i = 0; i < tabs.size();) {
        std::vector<std::shared_ptr<TerminalSession>> sessList;
        tabs[i]->rootNode->getSessions(sessList);
        bool anyAlive = false;
        for (const auto& s : sessList) {
            if (s->isAlive()) anyAlive = true;
        }
        if (!anyAlive && !sessList.empty()) {
            tabs.erase(tabs.begin() + i);
            activeTabIndex = std::clamp(activeTabIndex, 0, static_cast<int>(tabs.size()) - 1);
        } else {
            i++;
        }
    }

    if (tabs.empty()) {
        createTerminal("Terminal 1", selectedProfileName);
    }

    // 1. Menu Bar Layout
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Profiles")) {
            for (const auto& p : TerminalProfileManager::getInstance().profiles) {
                if (ImGui::MenuItem(p.name.c_str(), nullptr, selectedProfileName == p.name)) {
                    selectedProfileName = p.name;
                }
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Git")) {
            if (ImGui::MenuItem("git status")) UnrealGitHelper::runGitCommand(focusedSession, "status");
            if (ImGui::MenuItem("git add .")) UnrealGitHelper::runGitCommand(focusedSession, "add");
            if (ImGui::MenuItem("git commit")) UnrealGitHelper::runGitCommand(focusedSession, "commit");
            if (ImGui::MenuItem("git push")) UnrealGitHelper::runGitCommand(focusedSession, "push");
            if (ImGui::MenuItem("git pull")) UnrealGitHelper::runGitCommand(focusedSession, "pull");
            if (ImGui::MenuItem("git branch")) UnrealGitHelper::runGitCommand(focusedSession, "branch");
            ImGui::EndMenu();
        }


        if (ImGui::BeginMenu("Layout")) {
            if (ImGui::MenuItem("Split Horizontal")) createSplit(focusedNode, false);
            if (ImGui::MenuItem("Split Vertical")) createSplit(focusedNode, true);
            if (ImGui::MenuItem("Close Terminal")) killTerminal(focusedSession);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Options")) {
            ImGui::SliderFloat("Font Size", &fontSize, 8.0f, 24.0f, "%.1f px");
            ImGui::SliderFloat("Line Spacing", &lineSpacing, 1.0f, 2.0f, "%.2f");
            ImGui::EndMenu();
        }

        // Search Bar Toggle
        ImGui::SameLine(ImGui::GetWindowWidth() - 120.0f);
        if (ImGui::Button("Find")) {
            showSearchBar = !showSearchBar;
        }

        if (ImGui::Button("Help")) {
            showHelpPopup = true;
        }

        ImGui::EndMenuBar();
    }

    // Search bar panel render
    if (showSearchBar) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::Text("Find: "); ImGui::SameLine();
        ImGui::PushItemWidth(150);
        if (ImGui::InputText("##searchTerm", searchBuffer, sizeof(searchBuffer)) && strlen(searchBuffer) > 0) {
            performSearch();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Checkbox("Match Case", &searchCaseSensitive) && strlen(searchBuffer) > 0) {
            performSearch();
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Regex", &searchRegex) && strlen(searchBuffer) > 0) {
            performSearch();
        }
        ImGui::SameLine();
        if (ImGui::Button("Prev")) {
            if (!searchMatches.empty()) {
                activeSearchMatchIndex = (activeSearchMatchIndex - 1 + searchMatches.size()) % searchMatches.size();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Next")) {
            if (!searchMatches.empty()) {
                activeSearchMatchIndex = (activeSearchMatchIndex + 1) % searchMatches.size();
            }
        }
        if (!searchMatches.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.4f, 1.0f), "%d / %d", activeSearchMatchIndex + 1, (int)searchMatches.size());
        }
        ImGui::PopStyleVar();
        ImGui::Separator();
    }

    // 2. Tab Bar Header
    ImGuiTabBarFlags tabFlags = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs;
    if (ImGui::BeginTabBar("TerminalTabs", tabFlags)) {
        for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
            bool openTab = true;
            std::string label = tabs[i]->name + "##tab_" + std::to_string(i);
            
            if (ImGui::BeginTabItem(label.c_str(), &openTab)) {
                activeTabIndex = i;
                renderTabUI(tabs[i]);
                ImGui::EndTabItem();
            }
            
            if (!openTab) {
                // Close tab
                std::vector<std::shared_ptr<TerminalSession>> list;
                tabs[i]->rootNode->getSessions(list);
                for (const auto& s : list) {
                    s->kill();
                }
                tabs.erase(tabs.begin() + i);
                activeTabIndex = std::clamp(activeTabIndex, 0, static_cast<int>(tabs.size()) - 1);
                break;
            }
        }
        
        // Draw "+" tab button to create new terminal session
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
            createTerminal("", selectedProfileName);
        }
        ImGui::EndTabBar();
    }

    // Help popup
    if (showHelpPopup) {
        ImGui::OpenPopup("Terminal Shortcuts Help");
        showHelpPopup = false;
    }
    if (ImGui::BeginPopupModal("Terminal Shortcuts Help", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Integrated Terminal keyboard shortcuts:");
        ImGui::Separator();
        ImGui::Text("• Ctrl+C: Send interrupt / Copy selected text");
        ImGui::Text("• Ctrl+V: Paste from clipboard");
        ImGui::Text("• PageUp/PageDown: Scroll viewport");
        ImGui::Text("• Up/Down/Left/Right Arrows: Navigate shell history & inputs");
        ImGui::Text("• Right Click: Open terminal context actions menu");
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    ImGui::End();
}

void TerminalPanel::renderTabUI(std::shared_ptr<TerminalTab> tab) {
    if (!tab) return;
    
    ImVec2 available = ImGui::GetContentRegionAvail();
    // Leave room for vertical layouts
    if (available.y < 100.0f) available.y = 100.0f;

    renderSplitNodeUI(tab->rootNode, available, "tab_" + tab->name + "_root");
}

void TerminalPanel::renderSplitNodeUI(std::shared_ptr<TerminalSplitNode> node, ImVec2 size, const std::string& idPrefix) {
    if (!node) return;

    if (node->isSplit) {
        ImVec2 sizeA = size;
        ImVec2 sizeB = size;

        // Custom split partition math
        if (node->splitVertical) {
            sizeA.x = size.x * node->splitRatio - 2.5f;
            sizeB.x = size.x * (1.0f - node->splitRatio) - 2.5f;

            ImGui::BeginChild((idPrefix + "_splitA").c_str(), sizeA, false, ImGuiWindowFlags_NoScrollbar);
            renderSplitNodeUI(node->childA, sizeA, idPrefix + "_A");
            ImGui::EndChild();

            ImGui::SameLine();

            // Interactive splitter bar
            ImGui::Button((idPrefix + "_sep").c_str(), ImVec2(5.0f, size.y));
            if (ImGui::IsItemActive()) {
                node->splitRatio += ImGui::GetIO().MouseDelta.x / size.x;
                node->splitRatio = std::clamp(node->splitRatio, 0.1f, 0.9f);
            }
            
            ImGui::SameLine();

            ImGui::BeginChild((idPrefix + "_splitB").c_str(), sizeB, false, ImGuiWindowFlags_NoScrollbar);
            renderSplitNodeUI(node->childB, sizeB, idPrefix + "_B");
            ImGui::EndChild();
        } else {
            sizeA.y = size.y * node->splitRatio - 2.5f;
            sizeB.y = size.y * (1.0f - node->splitRatio) - 2.5f;

            ImGui::BeginChild((idPrefix + "_splitA").c_str(), ImVec2(size.x, sizeA.y), false, ImGuiWindowFlags_NoScrollbar);
            renderSplitNodeUI(node->childA, ImVec2(size.x, sizeA.y), idPrefix + "_A");
            ImGui::EndChild();

            // Interactive splitter bar
            ImGui::Button((idPrefix + "_sep").c_str(), ImVec2(size.x, 5.0f));
            if (ImGui::IsItemActive()) {
                node->splitRatio += ImGui::GetIO().MouseDelta.y / size.y;
                node->splitRatio = std::clamp(node->splitRatio, 0.1f, 0.9f);
            }

            ImGui::BeginChild((idPrefix + "_splitB").c_str(), ImVec2(size.x, sizeB.y), false, ImGuiWindowFlags_NoScrollbar);
            renderSplitNodeUI(node->childB, ImVec2(size.x, sizeB.y), idPrefix + "_B");
            ImGui::EndChild();
        }
    } else {
        renderSessionGridUI(node->session, size, idPrefix + "_leaf");
        if (focusedSession == node->session) {
            focusedNode = node;
        }
    }
}

void TerminalPanel::renderSessionGridUI(std::shared_ptr<TerminalSession> session, ImVec2 size, const std::string& idPrefix) {
    if (!session) return;

    // Outer panel wrapper
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
    
    // Flag window interaction
    ImGuiWindowFlags childFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::BeginChild(idPrefix.c_str(), size, true, childFlags);
    
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        focusedSession = session;
    }

    // Draw active background status
    if (!session->isAlive()) {
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "Terminal process died. Click to close or restart.");
        if (ImGui::Button("Close")) {
            killTerminal(session);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    TerminalEmulator& em = session->getEmulator();

    // Pull configurations
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    
    ImVec2 contentPos = ImGui::GetCursorScreenPos();
    ImVec2 maxRegion = ImGui::GetContentRegionAvail();

    // Font metrics calculation
    float charWidth = ImGui::CalcTextSize("A").x + letterSpacing;
    float charHeight = ImGui::GetTextLineHeight() * lineSpacing;
    if (charWidth <= 0.0f) charWidth = 1.0f;
    if (charHeight <= 0.0f) charHeight = 1.0f;
    
    // Determine terminal column/row capacity
    int cols = std::max(5, static_cast<int>(maxRegion.x / charWidth));
    int rows = std::max(2, static_cast<int>(maxRegion.y / charHeight));

    // Handle resizing on capacity change (before locking emulator to avoid recursive lock deadlock)
    if (cols != em.getCols() || rows != em.getRows()) {
        session->resize(cols, rows);
    }

    // Focus input intercept (before locking emulator to avoid recursive deadlock)
    if (focusedSession == session) {
        handleKeyboardInput(session);
    }

    // Capture mouse scroll wheel for scrollback history (before locking emulator to avoid recursive deadlock)
    float mouseWheel = ImGui::GetIO().MouseWheel;
    if (ImGui::IsWindowHovered() && mouseWheel != 0.0f) {
        em.scrollViewport(static_cast<int>(-mouseWheel * 3)); // 3 lines per wheel notch
    }

    em.lock();

    const auto& primaryGrid = em.getPrimaryGrid();
    const auto& altGrid = em.getAltGrid();
    bool altActive = em.isAltBufferActive();
    int gridHeight = altActive ? static_cast<int>(altGrid.size()) : static_cast<int>(primaryGrid.size());
    int scrollbackOffset = em.getScrollbackOffset();

    // Render cells
    for (int y = 0; y < rows; ++y) {
        int activeY = altActive ? y : (scrollbackOffset + y);
        if (activeY < 0 || activeY >= gridHeight) continue;
        
        const auto& rowCells = altActive ? altGrid[y] : primaryGrid[activeY];
        
        // Draw background runs to minimize ImGui Draw commands
        int runStart = 0;
        uint32_t currentBg = 0x1A1A1AFF;
        bool hasRun = false;
        
        for (int x = 0; x < cols; ++x) {
            uint32_t cellBg = rowCells[x].bg;
            if (isCellSelected(session, x, activeY)) {
                cellBg = 0x336699BB; // Selection highlight
            }
            
            if (!hasRun) {
                runStart = x;
                currentBg = cellBg;
                hasRun = true;
            } else if (cellBg != currentBg) {
                if (currentBg != 0x1A1A1AFF && currentBg != 0) {
                    ImVec2 minPos(contentPos.x + runStart * charWidth, contentPos.y + y * charHeight);
                    ImVec2 maxPos(contentPos.x + x * charWidth, minPos.y + charHeight);
                    drawList->AddRectFilled(minPos, maxPos, currentBg);
                }
                runStart = x;
                currentBg = cellBg;
            }
        }
        if (hasRun && currentBg != 0x1A1A1AFF && currentBg != 0) {
            ImVec2 minPos(contentPos.x + runStart * charWidth, contentPos.y + y * charHeight);
            ImVec2 maxPos(contentPos.x + cols * charWidth, minPos.y + charHeight);
            drawList->AddRectFilled(minPos, maxPos, currentBg);
        }

        // Draw Search Match highlights
        for (const auto& match : searchMatches) {
            if (match.line == activeY) {
                ImVec2 minPos(contentPos.x + match.startX * charWidth, contentPos.y + y * charHeight);
                ImVec2 maxPos(contentPos.x + match.endX * charWidth, minPos.y + charHeight);
                drawList->AddRect(minPos, maxPos, 0xFF00FFFF, 0.0f, 0, 1.5f); // Orange border
            }
        }

        // Draw text cells
        for (int x = 0; x < cols; ++x) {
            char32_t cp = rowCells[x].codepoint;
            if (cp == ' ' || cp == 0) continue;
            
            std::string textRun = utf32_to_utf8(cp);
            ImVec2 textPos(contentPos.x + x * charWidth, contentPos.y + y * charHeight);
            
            uint32_t fg = rowCells[x].fg;
            if (rowCells[x].attributes & TerminalCell::Attr_Bold) {
                fg |= 0x15151500; // Brighten slightly
            }

            drawList->AddText(font, fontSize, textPos, fg, textRun.c_str());
            
            if (rowCells[x].attributes & TerminalCell::Attr_Underline) {
                drawList->AddLine(
                    ImVec2(textPos.x, textPos.y + charHeight - 2.0f),
                    ImVec2(textPos.x + charWidth, textPos.y + charHeight - 2.0f),
                    fg,
                    1.0f
                );
            }
        }
    }

    // Draw active cursor block
    int cursorX = em.getCursorX();
    int cursorY = em.getCursorY();
    if (cursorX >= 0 && cursorX < cols && cursorY >= 0 && cursorY < rows) {
        ImVec2 minPos(contentPos.x + cursorX * charWidth, contentPos.y + cursorY * charHeight);
        ImVec2 maxPos(minPos.x + charWidth, minPos.y + charHeight);
        
        // Draw green semi-transparent cursor block
        drawList->AddRectFilled(minPos, maxPos, 0x4400FF00);
        // White border
        drawList->AddRect(minPos, maxPos, 0xFFFFFFFF);
    }

    // Capture mouse actions for text selections
    ImVec2 mousePos = ImGui::GetMousePos();
    if (ImGui::IsWindowHovered()) {
        if (ImGui::IsMouseClicked(0)) {
            int cellX = static_cast<int>((mousePos.x - contentPos.x) / charWidth);
            int cellY = static_cast<int>((mousePos.y - contentPos.y) / charHeight);
            cellX = std::clamp(cellX, 0, cols - 1);
            cellY = std::clamp(cellY, 0, rows - 1);
            
            selectStartX = cellX;
            selectStartY = altActive ? cellY : (scrollbackOffset + cellY);
            selectEndX = selectStartX;
            selectEndY = selectStartY;
            isSelecting = true;
            selectionSession = session;
        }
        
        if (isSelecting && ImGui::IsMouseDragging(0)) {
            int cellX = static_cast<int>((mousePos.x - contentPos.x) / charWidth);
            int cellY = static_cast<int>((mousePos.y - contentPos.y) / charHeight);
            cellX = std::clamp(cellX, 0, cols - 1);
            cellY = std::clamp(cellY, 0, rows - 1);
            
            selectEndX = cellX;
            selectEndY = altActive ? cellY : (scrollbackOffset + cellY);
        }
        
        if (isSelecting && ImGui::IsMouseReleased(0)) {
            isSelecting = false;
        }
    }

    em.unlock();

    // Context Actions Menu triggers
    showContextMenu(session, focusedNode);

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void TerminalPanel::handleKeyboardInput(std::shared_ptr<TerminalSession> session) {
    ImGuiIO& io = ImGui::GetIO();
    
    // Intercept Shift navigation keys (for scrolling terminal history)
    if (io.KeyShift) {
        TerminalEmulator& em = session->getEmulator();
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
            em.scrollViewport(em.getRows() / 2);
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
            em.scrollViewport(-em.getRows() / 2);
            return;
        }
    }

    // Intercept Control keystrokes
    if (io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_C)) {
            if (selectStartX != -1 && selectEndX != -1 && selectionSession == session) {
                copySelectedText(session);
            } else {
                session->writeInput("\x03"); // Cancel command (SIGINT)
            }
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_V)) {
            pasteFromClipboard(session);
            return;
        }
    }

    // Intercept character sequences
    for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
        ImWchar c = io.InputQueueCharacters[i];
        if (c >= 32 && c != 127) {
            std::string inputStr = utf32_to_utf8(c);
            session->writeInput(inputStr);
        }
    }

    // Clear input character queue so it is not parsed twice
    io.InputQueueCharacters.resize(0);

    // Map navigation keys
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
        session->writeInput("\r");
    } else if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        session->writeInput("\x7f");
    } else if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        session->writeInput("\t");
    } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        session->writeInput("\x1b");
    } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        session->writeInput("\x1b[A");
    } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        session->writeInput("\x1b[B");
    } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        session->writeInput("\x1b[C");
    } else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        session->writeInput("\x1b[D");
    } else if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
        session->writeInput("\x1b[H");
    } else if (ImGui::IsKeyPressed(ImGuiKey_End)) {
        session->writeInput("\x1b[F");
    } else if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
        session->writeInput("\x1b[5~");
    } else if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
        session->writeInput("\x1b[6~");
    } else if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        session->writeInput("\x1b[3~");
    }
}

void TerminalPanel::showContextMenu(std::shared_ptr<TerminalSession> session, std::shared_ptr<TerminalSplitNode> node) {
    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::MenuItem("Copy", "Ctrl+C")) {
            copySelectedText(session);
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V")) {
            pasteFromClipboard(session);
        }
        ImGui::Separator();
        
        if (ImGui::MenuItem("Split Horizontal")) {
            createSplit(node, false);
        }
        if (ImGui::MenuItem("Split Vertical")) {
            createSplit(node, true);
        }
        if (ImGui::MenuItem("Kill Terminal")) {
            killTerminal(session);
        }
        ImGui::Separator();

        if (ImGui::MenuItem("Select All")) {
            TerminalEmulator& em = session->getEmulator();
            em.lock();
            selectStartX = 0;
            selectStartY = 0;
            selectEndX = em.getCols() - 1;
            selectEndY = static_cast<int>(em.getPrimaryGrid().size()) - 1;
            selectionSession = session;
            em.unlock();
        }
        if (ImGui::MenuItem("Clear Terminal")) {
            session->getEmulator().clearGrid();
        }
        
        ImGui::EndPopup();
    }
}

bool TerminalPanel::isCellSelected(std::shared_ptr<TerminalSession> session, int x, int y) {
    if (selectionSession != session || selectStartX == -1 || selectEndX == -1) return false;

    int startY = selectStartY;
    int endY = selectEndY;
    int startX = selectStartX;
    int endX = selectEndX;

    // Swap coordinates if selection is backwards
    if (startY > endY || (startY == endY && startX > endX)) {
        std::swap(startY, endY);
        std::swap(startX, endX);
    }

    if (y < startY || y > endY) return false;
    if (y > startY && y < endY) return true;
    if (startY == endY) {
        return (x >= startX && x <= endX);
    }
    if (y == startY) return (x >= startX);
    if (y == endY) return (x <= endX);

    return false;
}

void TerminalPanel::copySelectedText(std::shared_ptr<TerminalSession> session) {
    if (selectionSession != session || selectStartX == -1 || selectEndX == -1) return;

    TerminalEmulator& em = session->getEmulator();
    em.lock();

    int startY = selectStartY;
    int endY = selectEndY;
    int startX = selectStartX;
    int endX = selectEndX;

    if (startY > endY || (startY == endY && startX > endX)) {
        std::swap(startY, endY);
        std::swap(startX, endX);
    }

    const auto& primaryGrid = em.getPrimaryGrid();
    const auto& altGrid = em.getAltGrid();
    bool altActive = em.isAltBufferActive();
    int gridHeight = altActive ? static_cast<int>(altGrid.size()) : static_cast<int>(primaryGrid.size());

    std::string copiedText;
    for (int y = startY; y <= endY; ++y) {
        if (y < 0 || y >= gridHeight) continue;
        const auto& row = altActive ? altGrid[y] : primaryGrid[y];
        
        int lineStartX = (y == startY) ? startX : 0;
        int lineEndX = (y == endY) ? endX : em.getCols() - 1;

        std::string lineStr;
        for (int x = lineStartX; x <= lineEndX; ++x) {
            char32_t cp = row[x].codepoint;
            if (cp != 0) {
                lineStr += utf32_to_utf8(cp);
            }
        }
        
        // Trim trailing space from copied line
        size_t endTrim = lineStr.find_last_not_of(" \t\r\n");
        if (endTrim != std::string::npos) {
            lineStr = lineStr.substr(0, endTrim + 1);
        }
        
        copiedText += lineStr;
        if (y < endY) {
            copiedText += "\r\n";
        }
    }

    ImGui::SetClipboardText(copiedText.c_str());
    
    // Clear selection
    selectStartX = -1;
    selectEndX = -1;
    selectionSession = nullptr;

    em.unlock();
}

void TerminalPanel::pasteFromClipboard(std::shared_ptr<TerminalSession> session) {
    const char* clipboardText = ImGui::GetClipboardText();
    if (clipboardText) {
        session->writeInput(clipboardText);
    }
}

void TerminalPanel::performSearch() {
    searchMatches.clear();
    activeSearchMatchIndex = -1;

    if (!focusedSession || strlen(searchBuffer) == 0) return;

    TerminalEmulator& em = focusedSession->getEmulator();
    em.lock();

    const auto& primaryGrid = em.getPrimaryGrid();
    const auto& altGrid = em.getAltGrid();
    bool altActive = em.isAltBufferActive();
    int gridHeight = altActive ? static_cast<int>(altGrid.size()) : static_cast<int>(primaryGrid.size());

    for (int y = 0; y < gridHeight; ++y) {
        const auto& row = altActive ? altGrid[y] : primaryGrid[y];
        
        // Assemble line string
        std::string lineStr;
        for (int x = 0; x < em.getCols(); ++x) {
            char32_t cp = row[x].codepoint;
            lineStr += (cp == 0) ? " " : utf32_to_utf8(cp);
        }

        if (searchRegex) {
            try {
                std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
                if (!searchCaseSensitive) flags |= std::regex_constants::icase;
                
                std::regex re(searchBuffer, flags);
                auto matchesBegin = std::sregex_iterator(lineStr.begin(), lineStr.end(), re);
                auto matchesEnd = std::sregex_iterator();
                
                for (std::sregex_iterator i = matchesBegin; i != matchesEnd; ++i) {
                    std::smatch match = *i;
                    SearchMatch sm;
                    sm.line = y;
                    sm.startX = static_cast<int>(match.position());
                    sm.endX = static_cast<int>(match.position() + match.length());
                    searchMatches.push_back(sm);
                }
            } catch (...) {}
        } else {
            std::string needle = searchBuffer;
            std::string haystack = lineStr;
            if (!searchCaseSensitive) {
                std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
                std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
            }
            
            size_t pos = 0;
            while ((pos = haystack.find(needle, pos)) != std::string::npos) {
                SearchMatch sm;
                sm.line = y;
                sm.startX = static_cast<int>(pos);
                sm.endX = static_cast<int>(pos + needle.length());
                searchMatches.push_back(sm);
                pos += needle.length();
            }
        }
    }

    if (!searchMatches.empty()) {
        activeSearchMatchIndex = 0;
    }

    em.unlock();
}

void TerminalPanel::saveLayoutState() {
    // Save active terminals configuration to terminals.txt
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir = std::string(exePath).substr(0, std::string(exePath).find_last_of("\\/"));
    std::string layoutPath = exeDir + "\\terminals.txt";
    
    std::ofstream file(layoutPath);
    if (!file.is_open()) return;

    file << "activeTabIndex=" << activeTabIndex << "\n";
    file << "fontSize=" << fontSize << "\n";
    file << "lineSpacing=" << lineSpacing << "\n";

    // Serialize tabs
    file << "tabsCount=" << tabs.size() << "\n";
    for (size_t i = 0; i < tabs.size(); ++i) {
        file << "tab_" << i << "_name=" << tabs[i]->name << "\n";
        
        // Write layout tree (DFS serialization)
        auto serializeNode = [&](auto& self, std::shared_ptr<TerminalSplitNode> node, const std::string& prefix) -> void {
            if (!node) return;
            file << prefix << "_isSplit=" << (node->isSplit ? 1 : 0) << "\n";
            if (node->isSplit) {
                file << prefix << "_splitVertical=" << (node->splitVertical ? 1 : 0) << "\n";
                file << prefix << "_splitRatio=" << node->splitRatio << "\n";
                self(self, node->childA, prefix + "_A");
                self(self, node->childB, prefix + "_B");
            } else {
                std::string path(node->session->getShellPath().begin(), node->session->getShellPath().end());
                std::string args(node->session->getArguments().begin(), node->session->getArguments().end());
                std::string dir(node->session->getWorkingDir().begin(), node->session->getWorkingDir().end());
                
                file << prefix << "_shell=" << path << "\n";
                file << prefix << "_args=" << args << "\n";
                file << prefix << "_dir=" << dir << "\n";
            }
        };
        serializeNode(serializeNode, tabs[i]->rootNode, "tab_" + std::to_string(i));
    }
}

void TerminalPanel::loadLayoutState() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir = std::string(exePath).substr(0, std::string(exePath).find_last_of("\\/"));
    std::string layoutPath = exeDir + "\\terminals.txt";
    
    std::ifstream file(layoutPath);
    if (!file.is_open()) return;

    std::unordered_map<std::string, std::string> kvs;
    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            kvs[line.substr(0, eq)] = line.substr(eq + 1);
        }
    }

    if (kvs.find("fontSize") != kvs.end()) fontSize = std::stof(kvs["fontSize"]);
    if (kvs.find("lineSpacing") != kvs.end()) lineSpacing = std::stof(kvs["lineSpacing"]);
    if (kvs.find("activeTabIndex") != kvs.end()) activeTabIndex = std::stoi(kvs["activeTabIndex"]);

    int tabsCount = 0;
    if (kvs.find("tabsCount") != kvs.end()) tabsCount = std::stoi(kvs["tabsCount"]);
    
    tabs.clear();
    for (int i = 0; i < tabsCount; ++i) {
        std::string tabPrefix = "tab_" + std::to_string(i);
        std::string tabName = kvs[tabPrefix + "_name"];

        auto deserializeNode = [&](auto& self, const std::string& prefix) -> std::shared_ptr<TerminalSplitNode> {
            auto node = std::make_shared<TerminalSplitNode>();
            if (kvs.find(prefix + "_isSplit") == kvs.end()) return nullptr;

            bool isSplit = std::stoi(kvs[prefix + "_isSplit"]) == 1;
            if (isSplit) {
                node->isSplit = true;
                node->splitVertical = std::stoi(kvs[prefix + "_splitVertical"]) == 1;
                node->splitRatio = std::stof(kvs[prefix + "_splitRatio"]);
                node->childA = self(self, prefix + "_A");
                node->childB = self(self, prefix + "_B");
            } else {
                std::string path = kvs[prefix + "_shell"];
                std::string args = kvs[prefix + "_args"];
                std::string dir = kvs[prefix + "_dir"];

                std::wstring wPath(path.begin(), path.end());
                std::wstring wArgs(args.begin(), args.end());
                std::wstring wDir(dir.begin(), dir.end());

                auto session = std::make_shared<TerminalSession>(tabName, wPath, wArgs, wDir, envVariables);
                session->start(80, 24);
                
                node->isSplit = false;
                node->session = session;
            }
            return node;
        };

        auto root = deserializeNode(deserializeNode, tabPrefix);
        if (root) {
            auto tab = std::make_shared<TerminalTab>();
            tab->name = tabName;
            tab->rootNode = root;
            tabs.push_back(tab);
        }
    }

    if (!tabs.empty()) {
        activeTabIndex = std::clamp(activeTabIndex, 0, static_cast<int>(tabs.size()) - 1);
        std::vector<std::shared_ptr<TerminalSession>> sessList;
        tabs[activeTabIndex]->rootNode->getSessions(sessList);
        if (!sessList.empty()) {
            focusedSession = sessList[0];
        }
    }
}

} // namespace forge
