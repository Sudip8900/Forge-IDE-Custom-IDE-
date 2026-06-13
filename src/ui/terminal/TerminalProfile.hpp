#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace forge {

struct TerminalProfile {
    std::string name;
    std::string path;
    std::string args;
};

class TerminalProfileManager {
public:
    static TerminalProfileManager& getInstance() {
        static TerminalProfileManager instance;
        return instance;
    }

    std::vector<TerminalProfile> profiles;
    std::string defaultProfile = "PowerShell";

    void loadProfiles(const std::string& filepath) {
        profiles.clear();
        std::ifstream file(filepath);
        if (!file.is_open()) {
            createDefaultProfilesFile(filepath);
            return;
        }

        std::stringstream ss;
        ss << file.rdbuf();
        std::string content = ss.str();
        
        // Simple manual JSON parser
        // Extract default_profile
        size_t defPos = content.find("\"default_profile\"");
        if (defPos != std::string::npos) {
            size_t valStart = content.find("\"", defPos + 17);
            if (valStart != std::string::npos) {
                size_t valEnd = content.find("\"", valStart + 1);
                if (valEnd != std::string::npos) {
                    defaultProfile = content.substr(valStart + 1, valEnd - valStart - 1);
                }
            }
        }

        // Parse profiles array
        size_t arrayStart = content.find("\"profiles\"");
        if (arrayStart != std::string::npos) {
            size_t openBracket = content.find("[", arrayStart);
            if (openBracket != std::string::npos) {
                size_t closeBracket = content.find("]", openBracket);
                if (closeBracket != std::string::npos) {
                    std::string arrayContent = content.substr(openBracket + 1, closeBracket - openBracket - 1);
                    
                    // Parse individual objects { ... }
                    size_t objStart = 0;
                    while ((objStart = arrayContent.find("{", objStart)) != std::string::npos) {
                        size_t objEnd = arrayContent.find("}", objStart);
                        if (objEnd == std::string::npos) break;
                        
                        std::string objStr = arrayContent.substr(objStart + 1, objEnd - objStart - 1);
                        
                        TerminalProfile prof;
                        
                        // Parse name
                        size_t keyPos = objStr.find("\"name\"");
                        if (keyPos != std::string::npos) {
                            size_t vStart = objStr.find("\"", keyPos + 6);
                            if (vStart != std::string::npos) {
                                size_t vEnd = objStr.find("\"", vStart + 1);
                                if (vEnd != std::string::npos) {
                                    prof.name = objStr.substr(vStart + 1, vEnd - vStart - 1);
                                }
                            }
                        }
                        
                        // Parse path
                        keyPos = objStr.find("\"path\"");
                        if (keyPos != std::string::npos) {
                            size_t vStart = objStr.find("\"", keyPos + 6);
                            if (vStart != std::string::npos) {
                                size_t vEnd = objStr.find("\"", vStart + 1);
                                if (vEnd != std::string::npos) {
                                    prof.path = objStr.substr(vStart + 1, vEnd - vStart - 1);
                                    
                                    // Unescape backslashes in JSON
                                    std::string unescaped;
                                    for (size_t k = 0; k < prof.path.length(); ++k) {
                                        if (prof.path[k] == '\\' && k + 1 < prof.path.length() && prof.path[k+1] == '\\') {
                                            unescaped += '\\';
                                            k++;
                                        } else {
                                            unescaped += prof.path[k];
                                        }
                                    }
                                    prof.path = unescaped;
                                }
                            }
                        }

                        // Parse args
                        keyPos = objStr.find("\"args\"");
                        if (keyPos != std::string::npos) {
                            size_t vStart = objStr.find("\"", keyPos + 6);
                            if (vStart != std::string::npos) {
                                size_t vEnd = objStr.find("\"", vStart + 1);
                                if (vEnd != std::string::npos) {
                                    prof.args = objStr.substr(vStart + 1, vEnd - vStart - 1);
                                }
                            }
                        }

                        if (!prof.name.empty() && !prof.path.empty()) {
                            profiles.push_back(prof);
                        }
                        
                        objStart = objEnd + 1;
                    }
                }
            }
        }

        if (profiles.empty()) {
            createDefaultProfilesFile(filepath);
        }
    }

    void saveProfiles(const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return;

        file << "{\n";
        file << "  \"default_profile\": \"" << defaultProfile << "\",\n";
        file << "  \"profiles\": [\n";
        for (size_t i = 0; i < profiles.size(); ++i) {
            file << "    {\n";
            file << "      \"name\": \"" << profiles[i].name << "\",\n";
            file << "      \"path\": \"" << escapePath(profiles[i].path) << "\",\n";
            file << "      \"args\": \"" << profiles[i].args << "\"\n";
            file << "    }" << (i + 1 < profiles.size() ? "," : "") << "\n";
        }
        file << "  ]\n";
        file << "}\n";
    }

private:
    TerminalProfileManager() {}

    std::string escapePath(const std::string& path) {
        std::string res;
        for (char c : path) {
            if (c == '\\') {
                res += "\\\\";
            } else {
                res += c;
            }
        }
        return res;
    }

    void createDefaultProfilesFile(const std::string& filepath) {
        profiles.clear();
        profiles.push_back({"PowerShell", "powershell.exe", "-NoLogo"});
        profiles.push_back({"Command Prompt", "cmd.exe", ""});
        profiles.push_back({"Git Bash", "C:\\Program Files\\Git\\bin\\bash.exe", ""});
        profiles.push_back({"WSL (Default)", "wsl.exe", ""});
        
        defaultProfile = "PowerShell";
        saveProfiles(filepath);
    }
};

} // namespace forge
