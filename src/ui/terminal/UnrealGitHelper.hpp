#pragma once

#include "TerminalSession.hpp"
#include <string>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace forge {

class UnrealGitHelper {
public:
    static void runGitCommand(std::shared_ptr<TerminalSession> session, const std::string& cmd) {
        if (!session || !session->isAlive()) return;

        std::string command;
        if (cmd == "status") command = "git status";
        else if (cmd == "add") command = "git add .";
        else if (cmd == "commit") command = "git commit -m \"Update from ForgeIDE\"";
        else if (cmd == "push") command = "git push";
        else if (cmd == "pull") command = "git pull";
        else if (cmd == "branch") command = "git branch -a";
        else if (cmd == "checkout") command = "git checkout ";
        else return;

        // Write to terminal session (simulate user typing and pressing enter)
        session->writeInput(command + "\r\n");
    }
};

} // namespace forge
