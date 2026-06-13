#pragma once

#include "../Panel.hpp"
#include "../terminal/TerminalProfile.hpp"
#include "../terminal/TerminalSession.hpp"
#include "imgui.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace forge {

struct TerminalSplitNode {
  std::shared_ptr<TerminalSession> session;
  bool isSplit = false;
  bool splitVertical = true;
  float splitRatio = 0.5f;
  std::shared_ptr<TerminalSplitNode> childA;
  std::shared_ptr<TerminalSplitNode> childB;

  // Helper to find leaf sessions recursively
  void getSessions(std::vector<std::shared_ptr<TerminalSession>> &list) {
    if (!isSplit) {
      if (session)
        list.push_back(session);
    } else {
      if (childA)
        childA->getSessions(list);
      if (childB)
        childB->getSessions(list);
    }
  }
};

struct TerminalTab {
  std::string name;
  std::shared_ptr<TerminalSplitNode> rootNode;
};

class TerminalPanel : public Panel {
public:
  TerminalPanel();
  ~TerminalPanel() override;

  const char *getName() const override { return "Integrated Terminal"; }
  void render() override;

  // API Exposure for direct shell tasks
  std::shared_ptr<TerminalSession>
  createTerminal(const std::string &name = "",
                 const std::string &profileName = "",
                 const std::string &workingDir = "");
  void createSplit(std::shared_ptr<TerminalSplitNode> node, bool vertical);
  void runCommand(const std::string &cmdLine,
                  const std::string &workingDir = "");
  void killTerminal(std::shared_ptr<TerminalSession> session);
  void renameTerminal(std::shared_ptr<TerminalSession> session,
                      const std::string &newName);

  // Persistence functions
  void saveLayoutState();
  void loadLayoutState();

  inline static TerminalPanel *instance = nullptr;

private:
  void renderTabUI(std::shared_ptr<TerminalTab> tab);
  void renderSplitNodeUI(std::shared_ptr<TerminalSplitNode> node, ImVec2 size,
                         const std::string &idPrefix);
  void renderSessionGridUI(std::shared_ptr<TerminalSession> session,
                           ImVec2 size, const std::string &idPrefix);

  void handleKeyboardInput(std::shared_ptr<TerminalSession> session);
  void showContextMenu(std::shared_ptr<TerminalSession> session,
                       std::shared_ptr<TerminalSplitNode> node);

  // Search functions
  void performSearch();
  bool isCellSelected(std::shared_ptr<TerminalSession> session, int x, int y);
  void copySelectedText(std::shared_ptr<TerminalSession> session);
  void pasteFromClipboard(std::shared_ptr<TerminalSession> session);

  // List of active tabs
  std::vector<std::shared_ptr<TerminalTab>> tabs;
  int activeTabIndex = 0;

  // Focus tracking
  std::shared_ptr<TerminalSession> focusedSession = nullptr;
  std::shared_ptr<TerminalSplitNode> focusedNode = nullptr;

  // Terminal Font options
  float fontSize = 13.0f;
  float lineSpacing = 1.15f;
  float letterSpacing = 0.0f;

  // Profiles selection
  std::string selectedProfileName;

  // Search state variables
  char searchBuffer[256] = "";
  bool searchCaseSensitive = false;
  bool searchRegex = false;
  bool showSearchBar = false;

  struct SearchMatch {
    int line;
    int startX;
    int endX;
  };
  std::vector<SearchMatch> searchMatches;
  int activeSearchMatchIndex = -1;

  // Selection variables
  bool isSelecting = false;
  std::shared_ptr<TerminalSession> selectionSession = nullptr;
  int selectStartX = -1, selectStartY = -1;
  int selectEndX = -1, selectEndY = -1;

  // Global Environment variables
  std::unordered_map<std::wstring, std::wstring> envVariables;

  // Help menu toggle
  bool showHelpPopup = false;

  // Thread-safe command deferral
  struct DeferCommand {
      std::string cmdLine;
      std::string workingDir;
  };
  std::vector<DeferCommand> deferredCommands;
  std::mutex deferredCommandsMutex;

  void executeCommandInternal(const std::string &cmdLine, const std::string &workingDir);
};

} // namespace forge
