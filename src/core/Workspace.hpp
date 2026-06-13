#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace forge {

struct WorkspaceFile {
    std::string name;
    std::string path;
    bool is_directory = false;
    size_t size = 0;
    std::vector<WorkspaceFile> children;
};

class Workspace {
public:
    static Workspace& getInstance();

    bool openProject(const std::string& folderPath);
    void closeProject();

    std::string getProjectPath() const { return currentProjectPath; }
    std::string getProjectName() const { return projectName; }
    bool isProjectOpen() const { return !currentProjectPath.empty(); }

    // File Tree Cache & Background Scan
    WorkspaceFile getFileTree();
    void refreshFileTreeAsync();

    // Document Management
    std::string openDocument(const std::string& filePath);
    bool saveDocument(const std::string& filePath, const std::string& content);
    void closeDocument(const std::string& filePath);

    std::vector<std::string> getOpenDocuments() const;
    std::string getActiveDocument() const { return activeDocument; }
    void setActiveDocument(const std::string& filePath) { activeDocument = filePath; }

private:
    Workspace();
    ~Workspace() = default;
    Workspace(const Workspace&) = delete;
    Workspace& operator=(const Workspace&) = delete;

    void buildTreeRecursive(const std::string& path, WorkspaceFile& node);

    std::string currentProjectPath;
    std::string projectName;
    std::string activeDocument;

    // Cache members
    WorkspaceFile cachedTree;
    std::atomic<bool> isScanning{false};

    // Cache of open files content (path -> content)
    std::unordered_map<std::string, std::string> openFiles;
    mutable std::mutex workspaceMutex;
};

} // namespace forge
