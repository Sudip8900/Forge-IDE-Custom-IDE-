#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include "Workspace.hpp"
#include "../utils/Logger.hpp"
#include "../ui/UIManager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#endif

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

Workspace::Workspace() : isScanning(false) {}

Workspace& Workspace::getInstance() {
    static Workspace instance;
    return instance;
}

bool Workspace::openProject(const std::string& folderPath) {
    std::string path;
    {
        std::lock_guard<std::mutex> lock(workspaceMutex);
        
        if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
            FORGE_LOG_ERROR("Workspace", "Project path does not exist or is not a directory: " + folderPath);
            return false;
        }

        currentProjectPath = fs::absolute(folderPath).string();
        projectName = fs::path(currentProjectPath).filename().string();
        path = currentProjectPath;
        
        FORGE_LOG_INFO("Workspace", "Opened project: " + projectName + " at " + currentProjectPath);
        
        // Save last opened project path to settings.txt in the executable directory via UIManager
        UIManager::getInstance().saveSettings();
    }

    // Initial background scan to populate tree cache
    refreshFileTreeAsync();

    return true;
}

void Workspace::closeProject() {
    std::lock_guard<std::mutex> lock(workspaceMutex);
    FORGE_LOG_INFO("Workspace", "Closing project: " + projectName);
    
    currentProjectPath.clear();
    projectName.clear();
    activeDocument.clear();
    openFiles.clear();
    cachedTree = WorkspaceFile();
}

WorkspaceFile Workspace::getFileTree() {
    std::lock_guard<std::mutex> lock(workspaceMutex);
    return cachedTree;
}

void Workspace::refreshFileTreeAsync() {
    std::string path;
    std::string name;
    {
        std::lock_guard<std::mutex> lock(workspaceMutex);
        if (currentProjectPath.empty()) return;
        path = currentProjectPath;
        name = projectName;
    }

    if (isScanning.exchange(true)) {
        return; // Scanning already in progress
    }

    std::thread([this, path, name]() {
        WorkspaceFile newTree;
        newTree.name = name;
        newTree.path = path;
        newTree.is_directory = true;

        buildTreeRecursive(path, newTree);

        {
            std::lock_guard<std::mutex> lock(workspaceMutex);
            cachedTree = std::move(newTree);
        }

        isScanning = false;
    }).detach();
}

void Workspace::buildTreeRecursive(const std::string& path, WorkspaceFile& node) {
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            std::string name = entry.path().filename().string();
            
            // Ignore heavy IDE/SCCS and compiler folders to maintain extreme performance
            if (name == ".git" || name == ".vs" || name == ".forge" || 
                name == "Binaries" || name == "Intermediate" || name == "Saved" || 
                name == "Build" || name == "DerivedDataCache") {
                continue;
            }

            WorkspaceFile child;
            child.name = name;
            child.path = entry.path().string();
            child.is_directory = fs::is_directory(entry.path());
            
            if (!child.is_directory) {
                child.size = fs::file_size(entry.path());
            } else {
                buildTreeRecursive(child.path, child);
                
                // Sort children: directories first, then files alphabetically
                std::sort(child.children.begin(), child.children.end(), [](const WorkspaceFile& a, const WorkspaceFile& b) {
                    if (a.is_directory != b.is_directory) {
                        return a.is_directory > b.is_directory;
                    }
                    return a.name < b.name;
                });
            }

            node.children.push_back(child);
        }

        // Sort root children
        std::sort(node.children.begin(), node.children.end(), [](const WorkspaceFile& a, const WorkspaceFile& b) {
            if (a.is_directory != b.is_directory) {
                return a.is_directory > b.is_directory;
            }
            return a.name < b.name;
        });
    }
    catch (const std::exception& e) {
        FORGE_LOG_ERROR("Workspace", "Error traversing folder " + path + ": " + e.what());
    }
}

std::string Workspace::openDocument(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(workspaceMutex);
    
    // Check cache
    auto it = openFiles.find(filePath);
    if (it != openFiles.end()) {
        activeDocument = filePath;
        return it->second;
    }

    // Read from disk
    std::ifstream file(filePath);
    if (!file.is_open()) {
        FORGE_LOG_ERROR("Workspace", "Failed to open document: " + filePath);
        return "";
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    
    openFiles[filePath] = content;
    activeDocument = filePath;
    
    FORGE_LOG_INFO("Workspace", "Opened document and cached: " + filePath);
    return content;
}

bool Workspace::saveDocument(const std::string& filePath, const std::string& content) {
    {
        std::lock_guard<std::mutex> lock(workspaceMutex);
        
        std::ofstream file(filePath);
        if (!file.is_open()) {
            FORGE_LOG_ERROR("Workspace", "Failed to save document: " + filePath);
            return false;
        }

        file << content;
        file.close();

        openFiles[filePath] = content;
        
        FORGE_LOG_INFO("Workspace", "Saved document to disk: " + filePath);
    }
    
    // Trigger async refresh of cached file tree and assets
    refreshFileTreeAsync();

    return true;
}

void Workspace::closeDocument(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(workspaceMutex);
    openFiles.erase(filePath);
    if (activeDocument == filePath) {
        activeDocument = openFiles.empty() ? "" : openFiles.begin()->first;
    }
    FORGE_LOG_INFO("Workspace", "Closed document from cache: " + filePath);
}

std::vector<std::string> Workspace::getOpenDocuments() const {
    std::lock_guard<std::mutex> lock(workspaceMutex);
    std::vector<std::string> docPaths;
    for (const auto& pair : openFiles) {
        docPaths.push_back(pair.first);
    }
    return docPaths;
}

} // namespace forge
