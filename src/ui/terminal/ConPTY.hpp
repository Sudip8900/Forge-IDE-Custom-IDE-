#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <string>
#include <unordered_map>

namespace forge {

#ifdef _WIN32
// Define HPCON if not defined in older SDK headers
typedef void* HPCON;
#else
typedef void* HPCON;
#endif

class ConPTY {
public:
    ConPTY();
    ~ConPTY();

    bool create(int cols, int rows);
    bool startProcess(const std::wstring& shellPath, 
                      const std::wstring& arguments, 
                      const std::wstring& workingDir, 
                      const std::unordered_map<std::wstring, std::wstring>& env);
    
    void write(const std::string& data);
    std::string read();
    void resize(int cols, int rows);
    void kill();
    bool isAlive();

private:
    HPCON hPC = nullptr;
    HANDLE hProcess = nullptr;
    HANDLE hThread = nullptr;
    HANDLE hPipeInWrite = nullptr;
    HANDLE hPipeOutRead = nullptr;

    bool initConPTYFunctions();
};

} // namespace forge
