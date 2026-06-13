#include "TerminalSession.hpp"
#include <chrono>
#include <atomic>
#include <sstream>

namespace forge {

std::string TerminalSession::generateUniqueId() {
    static std::atomic<uint32_t> counter{0};
    return "term_session_" + std::to_string(++counter);
}

TerminalSession::TerminalSession(const std::string& name, 
                                 const std::wstring& shellPath, 
                                 const std::wstring& arguments, 
                                 const std::wstring& workingDir,
                                 const std::unordered_map<std::wstring, std::wstring>& env)
    : name(name), 
      shellPath(shellPath), 
      arguments(arguments), 
      workingDir(workingDir),
      envVariables(env),
      emulator(80, 24) { // Default initial sizes, will be resized on layout
    sessionId = generateUniqueId();
}

TerminalSession::~TerminalSession() {
    kill();
}

bool TerminalSession::start(int cols, int rows) {
    emulator.resize(cols, rows);

    if (!pty.create(cols, rows)) {
        return false;
    }

    if (!pty.startProcess(shellPath, arguments, workingDir, envVariables)) {
        pty.kill();
        return false;
    }

    isReaderRunning = true;
    readerThread = std::thread([this]() {
        while (isReaderRunning.load()) {
            if (!pty.isAlive()) {
                isReaderRunning = false;
                break;
            }
            
            std::string output = pty.read();
            if (!output.empty()) {
                emulator.write(output);
            } else {
                // ReadFile returned empty / failed because process died or pipe closed
                isReaderRunning = false;
                break;
            }
        }
    });

    return true;
}

void TerminalSession::writeInput(const std::string& text) {
    pty.write(text);
}

void TerminalSession::resize(int cols, int rows) {
    pty.resize(cols, rows);
    emulator.resize(cols, rows);
}

void TerminalSession::kill() {
    isReaderRunning = false;
    
    // Kill PTY process which closes output pipes and unblocks ReadFile
    pty.kill();
    
    if (readerThread.joinable()) {
        readerThread.join();
    }
}

bool TerminalSession::isAlive() {
    return isReaderRunning.load() && pty.isAlive();
}

} // namespace forge
