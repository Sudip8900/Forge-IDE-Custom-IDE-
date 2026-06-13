#include "ConPTY.hpp"
#include <iostream>
#include <vector>

namespace forge {

// Typedefs for ConPTY APIs
typedef HRESULT(WINAPI* PFNCREATEPSEUDOCONSOLE)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
typedef HRESULT(WINAPI* PFNRESIZEPSEUDOCONSOLE)(HPCON, COORD);
typedef VOID(WINAPI* PFNCLOSEPSEUDOCONSOLE)(HPCON);

static PFNCREATEPSEUDOCONSOLE pfnCreatePseudoConsole = nullptr;
static PFNRESIZEPSEUDOCONSOLE pfnResizePseudoConsole = nullptr;
static PFNCLOSEPSEUDOCONSOLE pfnClosePseudoConsole = nullptr;

ConPTY::ConPTY() {}

ConPTY::~ConPTY() {
    kill();
}

bool ConPTY::initConPTYFunctions() {
    static bool checked = false;
    static bool success = false;
    if (checked) return success;
    checked = true;

    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    if (hKernel) {
        pfnCreatePseudoConsole = (PFNCREATEPSEUDOCONSOLE)GetProcAddress(hKernel, "CreatePseudoConsole");
        pfnResizePseudoConsole = (PFNRESIZEPSEUDOCONSOLE)GetProcAddress(hKernel, "ResizePseudoConsole");
        pfnClosePseudoConsole = (PFNCLOSEPSEUDOCONSOLE)GetProcAddress(hKernel, "ClosePseudoConsole");
    }

    success = (pfnCreatePseudoConsole && pfnResizePseudoConsole && pfnClosePseudoConsole);
    return success;
}

bool ConPTY::create(int cols, int rows) {
    if (!initConPTYFunctions()) {
        std::cerr << "ConPTY Error: Pseudoconsole APIs not found (requires Win10 build 17763+)." << std::endl;
        return false;
    }

    HANDLE hPipeInRead = NULL;
    HANDLE hPipeOutWrite = NULL;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Create pipes
    if (!CreatePipe(&hPipeInRead, &hPipeInWrite, &sa, 0)) return false;
    if (!CreatePipe(&hPipeOutRead, &hPipeOutWrite, &sa, 0)) {
        CloseHandle(hPipeInRead);
        CloseHandle(hPipeInWrite);
        return false;
    }

    COORD size = { static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
    HRESULT hr = pfnCreatePseudoConsole(size, hPipeInRead, hPipeOutWrite, 0, &hPC);

    // The Pseudo Console owns these handles now, close them in the parent
    CloseHandle(hPipeInRead);
    CloseHandle(hPipeOutWrite);

    if (FAILED(hr)) {
        CloseHandle(hPipeInWrite);
        CloseHandle(hPipeOutRead);
        hPipeInWrite = nullptr;
        hPipeOutRead = nullptr;
        return false;
    }

    return true;
}

bool ConPTY::startProcess(const std::wstring& shellPath, 
                          const std::wstring& arguments, 
                          const std::wstring& workingDir, 
                          const std::unordered_map<std::wstring, std::wstring>& env) {
    if (!hPC) return false;

    // Allocate & initialize startup info attribute list
    SIZE_T attributeListSize = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attributeListSize);
    if (attributeListSize == 0) return false;

    std::vector<BYTE> attributeListBuffer(attributeListSize);
    PPROC_THREAD_ATTRIBUTE_LIST pAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attributeListBuffer.data());
    
    if (!InitializeProcThreadAttributeList(pAttributeList, 1, 0, &attributeListSize)) {
        return false;
    }

    if (!UpdateProcThreadAttribute(pAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC, sizeof(HPCON), NULL, NULL)) {
        DeleteProcThreadAttributeList(pAttributeList);
        return false;
    }

    STARTUPINFOEXW si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.lpAttributeList = pAttributeList;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Construct full command line
    std::wstring cmdLine = L"\"" + shellPath + L"\" " + arguments;
    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    // Setup Environment block
    std::vector<wchar_t> envBlock;
    if (!env.empty()) {
        for (const auto& [key, val] : env) {
            std::wstring pair = key + L"=" + val;
            envBlock.insert(envBlock.end(), pair.begin(), pair.end());
            envBlock.push_back(L'\0');
        }
        envBlock.push_back(L'\0'); // Null terminator for block
    }

    BOOL success = CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        TRUE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        envBlock.empty() ? NULL : envBlock.data(),
        workingDir.empty() ? NULL : workingDir.c_str(),
        &si.StartupInfo,
        &pi
    );

    DeleteProcThreadAttributeList(pAttributeList);

    if (!success) {
        return false;
    }

    hProcess = pi.hProcess;
    hThread = pi.hThread;
    return true;
}

void ConPTY::write(const std::string& data) {
    if (!hPipeInWrite) return;
    DWORD written = 0;
    WriteFile(hPipeInWrite, data.c_str(), static_cast<DWORD>(data.length()), &written, NULL);
}

std::string ConPTY::read() {
    if (!hPipeOutRead) return "";
    char buffer[4096];
    DWORD bytesRead = 0;
    if (ReadFile(hPipeOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        return std::string(buffer, bytesRead);
    }
    return "";
}

void ConPTY::resize(int cols, int rows) {
    if (hPC && pfnResizePseudoConsole) {
        COORD size = { static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
        pfnResizePseudoConsole(hPC, size);
    }
}

void ConPTY::kill() {
    if (hProcess) {
        TerminateProcess(hProcess, 1);
        CloseHandle(hProcess);
        hProcess = nullptr;
    }
    if (hThread) {
        CloseHandle(hThread);
        hThread = nullptr;
    }
    if (hPC && pfnClosePseudoConsole) {
        pfnClosePseudoConsole(hPC);
        hPC = nullptr;
    }
    if (hPipeInWrite) {
        CloseHandle(hPipeInWrite);
        hPipeInWrite = nullptr;
    }
    if (hPipeOutRead) {
        CloseHandle(hPipeOutRead);
        hPipeOutRead = nullptr;
    }
}

bool ConPTY::isAlive() {
    if (!hProcess) return false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(hProcess, &exitCode)) {
        return (exitCode == STILL_ACTIVE);
    }
    return false;
}

} // namespace forge
