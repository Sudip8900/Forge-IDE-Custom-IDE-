#include "Logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace forge {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.close();
    }
}

void Logger::init(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (!initialized) {
        logFile.open(logFilePath, std::ios::out | std::ios::app);
        initialized = true;
    }
}

void Logger::log(LogLevel level, const std::string& category, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    std::string timeStr = getCurrentTimeString();
    std::string levelStr = getLevelString(level);
    
    std::stringstream ss;
    ss << "[" << timeStr << "] [" << levelStr << "] [" << category << "] " << message << std::endl;
    std::string logLine = ss.str();

    // Print to console
    if (level == LogLevel::ERR) {
        std::cerr << logLine;
    } else {
        std::cout << logLine;
    }

    // Write to file
    if (initialized && logFile.is_open()) {
        logFile << logLine;
        logFile.flush();
    }
}

void Logger::debug(const std::string& category, const std::string& message) {
    log(LogLevel::DEBUG, category, message);
}

void Logger::info(const std::string& category, const std::string& message) {
    log(LogLevel::INFO, category, message);
}

void Logger::warn(const std::string& category, const std::string& message) {
    log(LogLevel::WARNING, category, message);
}

void Logger::error(const std::string& category, const std::string& message) {
    log(LogLevel::ERR, category, message);
}

std::string Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERR:     return "ERROR";
        default:                return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    // Thread-safe localtime conversion on Windows (localtime_s)
    struct tm timeInfo;
    localtime_s(&timeInfo, &time);

    std::stringstream ss;
    ss << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

} // namespace forge
