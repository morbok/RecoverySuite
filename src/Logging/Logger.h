#ifndef RECOVERYSUITE_LOGGING_LOGGER_H
#define RECOVERYSUITE_LOGGING_LOGGER_H

#include <string>
#include <memory>
#include <mutex>

namespace recoverysuite {
namespace logging {

/**
 * Log levels
 */
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

/**
 * Logger
 * A simple thread-safe logger for RecoverySuite.
 */
class Logger {
public:
    /**
     * Get the singleton instance of the logger.
     * @return Reference to the logger instance.
     */
    static Logger& instance();

    /**
     * Set the log level.
     * @param level The minimum level to log.
     */
    void setLogLevel(LogLevel level);

    /**
     * Log a message with the specified level.
     * @param level The log level.
     * @param message The message to log.
     */
    void log(LogLevel level, const std::string& message);

    /**
     * Convenience methods for each log level.
     */
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    Logger();
    ~Logger();

    // Disable copy and move
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex mutex_;
    LogLevel logLevel_;
    // In a more advanced implementation, we might have file output, etc.
    // For now, we'll log to stderr/stdout.
};

} // namespace logging
} // namespace recoverysuite

#endif // RECOVERYSUITE_LOGGING_LOGGER_H