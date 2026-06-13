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

    ImGui::Text("Workspace: %s", Workspace::getInstance().getProjectName().c_str());
    ImGui::TextDisabled("%s", Workspace::getInstance().getProjectPath().c_str());

    // File Explorer control buttons
    ImGui::Spacing();
    if (ImGui::Button("New File", ImVec2(75, 20))) {
        ImGui::OpenPopup("CreateNewFilePopup");
    }
    ImGui::SameLine();
    if (ImGui::Button("New Folder", ImVec2(85, 20))) {
        ImGui::OpenPopup("CreateNewFolderPopup");
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh", ImVec2(65, 20))) {
        Workspace::getInstance().refreshFileTreeAsync();
    }

    // Modal Popup: Create New File
    if (ImGui::BeginPopup("CreateNewFilePopup")) {
        static char newFileName[128] = "";
        ImGui::Text("File Name (relative to root):");
        ImGui::InputText("##newFileNameInput", newFileName, sizeof(newFileName));
        if (ImGui::Button("Create", ImVec2(80, 24)) && strlen(newFileName) > 0) {
            std::string prjPath = Workspace::getInstance().getProjectPath();
            std::string newFilePath = prjPath + "/" + newFileName;
            
            // Create parent folders if necessary (e.g. if name contains folders like src/core/main.cpp)
            fs::path p(newFilePath);
            if (p.has_parent_path()) {
                fs::create_directories(p.parent_path());
            }

            // Create empty file
            std::ofstream file(newFilePath);
            file.close();
            
            // Open and focus
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

    // Modal Popup: Create New Folder
    if (ImGui::BeginPopup("CreateNewFolderPopup")) {
        static char newFolderName[128] = "";
        ImGui::Text("Folder Name (relative to root):");
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

    ImGui::Separator();
    ImGui::Spacing();

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
    
    if (!node.is_directory) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        
        // Highlight active document in tree view
        if (Workspace::getInstance().getActiveDocument() == node.path) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::TreeNodeEx(node.name.c_str(), flags);
        if (ImGui::IsItemClicked() || 
            (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) ||
            (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))) {
            Workspace::getInstance().openDocument(node.path);
        }
    } else {
        // Collapsible directory folder node
        bool isOpen = ImGui::TreeNodeEx(node.name.c_str(), flags);
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
