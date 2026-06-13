#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

namespace forge {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERR
};

class Logger {
public:
    static Logger& getInstance();

    void init(const std::string& logFilePath);
    void log(LogLevel level, const std::string& category, const std::string& message);

    // Convenience functions
    void debug(const std::string& category, const std::string& message);
    void info(const std::string& category, const std::string& message);
    void warn(const std::string& category, const std::string& message);
    void error(const std::string& category, const std::string& message);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string getLevelString(LogLevel level);
    std::string getCurrentTimeString();

    std::ofstream logFile;
    std::mutex logMutex;
    bool initialized = false;
};

// Global logger helper macros
#define FORGE_LOG_DEBUG(category, msg) forge::Logger::getInstance().debug(category, msg)
#define FORGE_LOG_INFO(category, msg) forge::Logger::getInstance().info(category, msg)
#define FORGE_LOG_WARN(category, msg) forge::Logger::getInstance().warn(category, msg)
#define FORGE_LOG_ERROR(category, msg) forge::Logger::getInstance().error(category, msg)

} // namespace forge
