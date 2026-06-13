#pragma once

#include "../Panel.hpp"
#include <string>
#include <vector>

namespace forge {

class EditorPanel : public Panel {
public:
    EditorPanel() = default;
    ~EditorPanel() override = default;

    void render() override;
    const char* getName() const override { return "Code Editor"; }

private:
    void runActiveFile(const std::string& activeDocPath);
    
    std::string lastLoadedFile;
    std::vector<char> editBuffer;
    bool hasEdits = false;

    bool showRunnerSettingsPopup = false;
    char compileCmdBuffer[1024] = "";
    char runCmdBuffer[1024] = "";
};

} // namespace forge
