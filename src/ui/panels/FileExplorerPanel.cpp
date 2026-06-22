#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include "FileExplorerPanel.hpp"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <fstream>

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

void FileExplorerPanel::render() {
    ImGui::SetNextWindowSize(ImVec2(250, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(getName(), &open)) {
        ImGui::End();
        return;
    }

    if (!Workspace::getInstance().isProjectOpen()) {
        ImGui::TextDisabled("No active project.");
        ImGui::Spacing();
        if (ImGui::Button("Open Workspace Directory", ImVec2(-1, 24))) {
            Workspace::getInstance().openProject(fs::current_path().string());
        }
        ImGui::End();
        return;
    }

    // Modal Popup: Create New File
    bool openCreateFile = false;
    bool openCreateFolder = false;

    // Right click on explorer background for context menu
    if (ImGui::BeginPopupContextWindow("FileExplorerContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("New File...")) {
            openCreateFile = true;
        }
        if (ImGui::MenuItem("New Folder...")) {
            openCreateFolder = true;
        }
        if (ImGui::MenuItem("Refresh Workspace")) {
            Workspace::getInstance().refreshFileTreeAsync();
        }
        ImGui::EndPopup();
    }

    if (openCreateFile) ImGui::OpenPopup("CreateNewFilePopup");
    if (openCreateFolder) ImGui::OpenPopup("CreateNewFolderPopup");

    if (ImGui::BeginPopup("CreateNewFilePopup")) {
        static char newFileName[128] = "";
        ImGui::Text("File Name:");
        ImGui::InputText("##newFileNameInput", newFileName, sizeof(newFileName));
        if (ImGui::Button("Create", ImVec2(80, 24)) && strlen(newFileName) > 0) {
            std::string prjPath = Workspace::getInstance().getProjectPath();
            std::string newFilePath = prjPath + "/" + newFileName;
            fs::path p(newFilePath);
            if (p.has_parent_path()) {
                fs::create_directories(p.parent_path());
            }
            std::ofstream file(newFilePath);
            file.close();
            Workspace::getInstance().openDocument(newFilePath);
            Workspace::getInstance().refreshFileTreeAsync();
            memset(newFileName, 0, sizeof(newFileName));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 24))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("CreateNewFolderPopup")) {
        static char newFolderName[128] = "";
        ImGui::Text("Folder Name:");
        ImGui::InputText("##newFolderNameInput", newFolderName, sizeof(newFolderName));
        if (ImGui::Button("Create", ImVec2(80, 24)) && strlen(newFolderName) > 0) {
            std::string prjPath = Workspace::getInstance().getProjectPath();
            std::string newDirPath = prjPath + "/" + newFolderName;
            fs::create_directories(newDirPath);
            Workspace::getInstance().refreshFileTreeAsync();
            memset(newFolderName, 0, sizeof(newFolderName));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 24))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Trigger async background scanning every 2 seconds
    static double lastRefreshTime = 0.0;
    double currentTime = glfwGetTime();
    if (currentTime - lastRefreshTime > 2.0) {
        Workspace::getInstance().refreshFileTreeAsync();
        lastRefreshTime = currentTime;
    }

    WorkspaceFile root = Workspace::getInstance().getFileTree();
    
    ImGui::BeginChild("FileTreeContainer", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    for (const auto& child : root.children) {
        renderNode(child);
    }
    
    ImGui::EndChild();
    ImGui::End();
}

void FileExplorerPanel::renderNode(const WorkspaceFile& node) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    ImGui::PushID(node.path.c_str());
    
    bool isActive = (!node.is_directory && Workspace::getInstance().getActiveDocument() == node.path);
    
    if (isActive) {
        flags |= ImGuiTreeNodeFlags_Selected;
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.00f, 0.90f, 0.46f, 1.00f)); // Mint green active text
    }

    // Leave 3 spaces at the start of display name to draw the custom icon
    std::string displayName = "   " + node.name;
    
    if (!node.is_directory) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        ImGui::TreeNodeEx(displayName.c_str(), flags);
        
        // Draw custom file icon over the spaces
        ImVec2 itemPos = ImGui::GetItemRectMin();
        float textOffset = ImGui::GetTreeNodeToLabelSpacing();
        ImVec2 iconPos = ImVec2(itemPos.x + textOffset - 6.0f, itemPos.y + (ImGui::GetTextLineHeight() - 10.0f) * 0.5f + 1.0f);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImColor iconColor = isActive ? ImColor(0, 230, 118) : ImColor(156, 163, 175);
        
        if (node.name == "CMakeLists.txt") {
            // Draw gear icon
            drawList->AddCircle(ImVec2(iconPos.x + 4.5f, iconPos.y + 5.0f), 2.5f, iconColor, 12, 1.2f);
            for (int a = 0; a < 8; a++) {
                float angle = a * (3.14159f / 4.0f);
                drawList->AddLine(
                    ImVec2(iconPos.x + 4.5f + cos(angle) * 2.5f, iconPos.y + 5.0f + sin(angle) * 2.5f),
                    ImVec2(iconPos.x + 4.5f + cos(angle) * 4.5f, iconPos.y + 5.0f + sin(angle) * 4.5f),
                    iconColor, 1.2f
                );
            }
        } else {
            // Draw document outline
            drawList->AddRect(iconPos, ImVec2(iconPos.x + 8, iconPos.y + 11), iconColor, 0.0f, 0, 1.2f);
            drawList->AddLine(ImVec2(iconPos.x + 5, iconPos.y), ImVec2(iconPos.x + 8, iconPos.y + 3), iconColor, 1.2f);
        }

        if (ImGui::IsItemClicked() || 
            (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) ||
            (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))) {
            Workspace::getInstance().openDocument(node.path);
        }
        
        if (isActive) {
            ImGui::PopStyleColor();
        }
    } else {
        bool isOpen = ImGui::TreeNodeEx(displayName.c_str(), flags);
        
        // Draw folder icon over the spaces
        ImVec2 itemPos = ImGui::GetItemRectMin();
        float textOffset = ImGui::GetTreeNodeToLabelSpacing();
        ImVec2 iconPos = ImVec2(itemPos.x + textOffset - 6.0f, itemPos.y + (ImGui::GetTextLineHeight() - 10.0f) * 0.5f + 1.0f);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImColor folderColor = ImColor(0, 230, 118); // Green folders
        drawList->AddRectFilled(ImVec2(iconPos.x, iconPos.y + 2), ImVec2(iconPos.x + 10, iconPos.y + 9), folderColor, 1.0f);
        drawList->AddRectFilled(ImVec2(iconPos.x + 1, iconPos.y), ImVec2(iconPos.x + 5, iconPos.y + 3), folderColor, 1.0f);

        if (isActive) {
            ImGui::PopStyleColor();
        }

        if (isOpen) {
            for (const auto& child : node.children) {
                renderNode(child);
            }
            ImGui::TreePop();
        }
    }
    
    ImGui::PopID();
}

} // namespace forge
