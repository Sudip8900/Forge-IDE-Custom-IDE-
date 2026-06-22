#pragma once

#include "Panel.hpp"
#include <imgui.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace forge {

class UIManager {
public:
    struct EditorThemeColors {
        ImVec4 text;
        ImVec4 keyword;
        ImVec4 macro;
        ImVec4 type;
        ImVec4 string;
        ImVec4 comment;
        ImVec4 lineNumbers;
    };

    static UIManager& getInstance();

    void init();
    void shutdown();

    void addPanel(std::shared_ptr<Panel> panel);
    void renderAll(bool showMenuBar = true);

    // Theme swaps
    void applyThemeByName(const std::string& name);
    void applySlateDarkTheme();
    void applyCyberpunkTheme();
    void applyMonokaiTheme();
    void applyGithubLightTheme();

    std::string getActiveThemeName() const { return activeThemeName; }
    EditorThemeColors getEditorThemeColors() const { return editorTheme; }

    // Custom layout docking
    void createMainDockSpace();

    // Workspace presets
    void saveLayout(const std::string& presetName);
    void loadLayout(const std::string& presetName);

    // Persistence settings
    void saveSettings();
    void loadSettings();

    // Custom runner configurations
    void setCustomCompileCommand(const std::string& ext, const std::string& cmd);
    std::string getCustomCompileCommand(const std::string& ext);
    void setCustomRunCommand(const std::string& ext, const std::string& cmd);
    std::string getCustomRunCommand(const std::string& ext);

    const std::unordered_map<std::string, std::string>& getCustomCompileCommands() const { return customCompileCommands; }
    const std::unordered_map<std::string, std::string>& getCustomRunCommands() const { return customRunCommands; }

    std::vector<std::shared_ptr<Panel>> getPanels() { return panels; }

    void setCursorPos(int line, int col) { cursorLine = line; cursorCol = col; }
    int getCursorLine() const { return cursorLine; }
    int getCursorCol() const { return cursorCol; }

    bool resetLayout = false;
    float editorFontScale = 1.0f;

private:
    UIManager() = default;
    ~UIManager() = default;
    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;

    std::vector<std::shared_ptr<Panel>> panels;
    bool themeApplied = false;

    std::string activeThemeName = "Slate Dark";
    EditorThemeColors editorTheme;

    std::unordered_map<std::string, std::string> customCompileCommands;
    std::unordered_map<std::string, std::string> customRunCommands;

    int cursorLine = 1;
    int cursorCol = 1;
};

} // namespace forge
