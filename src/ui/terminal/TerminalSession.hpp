#pragma once

#include "ConPTY.hpp"
#include "TerminalEmulator.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <unordered_map>

namespace forge {

class TerminalSession {
public:
    TerminalSession(const std::string& name, 
                    const std::wstring& shellPath, 
                    const std::wstring& arguments, 
                    const std::wstring& workingDir,
                    const std::unordered_map<std::wstring, std::wstring>& env);
    ~TerminalSession();

    // Start shell reader thread
    bool start(int cols, int rows);
    
    // Send input to the process (keystrokes, text pastes)
    void writeInput(const std::string& text);
    
    // Request window buffer resize
    void resize(int cols, int rows);
    
    void kill();

    // Queries
    std::string getName() const { return name; }
    void setName(const std::string& n) { name = n; }
    
    std::string getSessionId() const { return sessionId; }
    std::wstring getWorkingDir() const { return workingDir; }
    std::wstring getShellPath() const { return shellPath; }
    std::wstring getArguments() const { return arguments; }

    TerminalEmulator& getEmulator() { return emulator; }
    bool isAlive();

private:
    std::string name;
    std::string sessionId;
    std::wstring shellPath;
    std::wstring arguments;
    std::wstring workingDir;
    std::unordered_map<std::wstring, std::wstring> envVariables;

    ConPTY pty;
    TerminalEmulator emulator;
    
    std::thread readerThread;
    std::atomic<bool> isReaderRunning{false};

    static std::string generateUniqueId();
};

} // namespace forge
