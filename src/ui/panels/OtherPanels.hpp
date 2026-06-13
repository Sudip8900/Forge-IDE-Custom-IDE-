#pragma once
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "../../core/Workspace.hpp"
#include "../Panel.hpp"
#include "../UIManager.hpp"
#include <GLFW/glfw3.h>
#include <atomic>
#include <fstream>
#include <imgui.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

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

#include "TerminalPanel.hpp"

namespace forge {

// ==========================================
// 1. Terminal Panel (Redirected to the new modular subsystem)
// ==========================================

// ==========================================
// 2. Output Panel
// ==========================================
class OutputPanel : public Panel {
public:
  OutputPanel() { open = false; }
  ~OutputPanel() override = default;

  const char *getName() const override { return "Output Window"; }

  inline static std::mutex staticOutputMutex;
  inline static std::string staticOutputLogs;
  inline static bool openRequested = false;

  static void appendLog(const std::string &text) {
    std::lock_guard<std::mutex> lock(staticOutputMutex);
    staticOutputLogs += text;
    openRequested = true;
  }

  bool isOpen() const override {
    if (openRequested) {
      const_cast<OutputPanel *>(this)->open = true;
      const_cast<OutputPanel *>(this)->openRequested = false;
    }
    return open;
  }

  void render() override {
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(getName(), &open)) {
      ImGui::End();
      return;
    }

    // Pull from static background logs
    {
      std::lock_guard<std::mutex> lock(staticOutputMutex);
      if (!staticOutputLogs.empty()) {
        outputLogs += staticOutputLogs;
        staticOutputLogs.clear();
        scrollToBottom = true;
      }
      if (openRequested) {
        open = true;
        openRequested = false;
      }
    }

    // Toolbar
    if (ImGui::Button("Clear")) {
      outputLogs.clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-Scroll", &autoScroll);
    ImGui::SameLine();
    if (ImGui::Button("Copy All")) {
      ImGui::SetClipboardText(outputLogs.c_str());
    }

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("OutputTextChild", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    // Fast line-by-line render with simple error highlighting
    std::stringstream ss(outputLogs);
    std::string line;
    while (std::getline(ss, line)) {
      ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text); // Default
      if (line.find("error") != std::string::npos ||
          line.find("Failed") != std::string::npos ||
          line.find("FAILED") != std::string::npos ||
          line.find("Error:") != std::string::npos) {
        color = ImVec4(0.9f, 0.3f, 0.3f, 1.0f); // Red
      } else if (line.find("warning") != std::string::npos ||
                 line.find("Warning:") != std::string::npos) {
        color = ImVec4(0.9f, 0.7f, 0.2f, 1.0f); // Yellow
      } else if (line.find("Succeeded") != std::string::npos ||
                 line.find("SUCCESSFUL") != std::string::npos ||
                 line.find("success") != std::string::npos) {
        color = ImVec4(0.3f, 0.8f, 0.4f, 1.0f); // Green
      }
      ImGui::TextColored(color, "%s", line.c_str());
    }

    if (scrollToBottom && autoScroll) {
      ImGui::SetScrollHereY(1.0f);
      scrollToBottom = false;
    }
    ImGui::EndChild();

    ImGui::End();
  }

private:
  std::string outputLogs;
  bool scrollToBottom = true;
  bool autoScroll = true;
};

} // namespace forge
