#include "Logger.h"
#include <iostream>
#include <sstream>

namespace recoverysuite {
namespace logging {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger() : logLevel_(LogLevel::INFO) {
    // Default to INFO level
}

Logger::~Logger() {
    // Cleanup if needed
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    logLevel_ = level;
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if the message should be logged
    if (static_cast<int>(level) < static_cast<int>(logLevel_)) {
        return;
    }

    // Format the message with a level prefix
    std::string levelStr;
    switch (level) {
        case LogLevel::DEBUG:
            levelStr = "DEBUG";
            break;
        case LogLevel::INFO:
            levelStr = "INFO";
            break;
        case LogLevel::WARN:
            levelStr = "WARN";
            break;
        case LogLevel::ERROR:
            levelStr = "ERROR";
            break;
    }

    // Output to stderr for WARN and ERROR, stdout for INFO and DEBUG
    if (level == LogLevel::ERROR || level == LogLevel::WARN) {
        std::cerr << "[" << levelStr << "] " << message << std::endl;
    } else {
        std::cout << "[" << levelStr << "] " << message << std::endl;
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::WARN, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

} // namespace logging
} // namespace recoverysuite