#pragma once

#include <stdexcept>
#include <string>
#include <system_error>
#include "Core/Version.h"

namespace recoverysuite {
namespace disk {

/**
 * @brief Base exception class for all disk-related errors
 */
class DiskException : public std::runtime_error {
public:
    explicit DiskException(const std::string& message)
        : std::runtime_error(message) {}

    DiskException(const std::string& message, int errorCode)
        : std::runtime_error(message), m_errorCode(errorCode) {}

    int getErrorCode() const { return m_errorCode; }

private:
    int m_errorCode = 0;
};

/**
 * @brief Exception thrown when a disk operation fails due to invalid parameters
 */
class InvalidParameterException : public DiskException {
public:
    explicit InvalidParameterException(const std::string& message)
        : DiskException(message) {}

    InvalidParameterException(const std::string& message, int errorCode)
        : DiskException(message, errorCode) {}
};

/**
 * @brief Exception thrown when a disk is not found or inaccessible
 */
class DiskNotFoundException : public DiskException {
public:
    explicit DiskNotFoundException(const std::string& message)
        : DiskException(message) {}

    DiskNotFoundException(const std::string& message, int errorCode)
        : DiskException(message, errorCode) {};
};

/**
 * @brief Exception thrown when a disk access is denied (often due to permissions)
 */
class AccessDeniedException : public DiskException {
public:
    explicit AccessDeniedException(const std::string& message)
        : DiskException(message) {}

    AccessDeniedException(const std::string& message, int errorCode)
        : DiskException(message, errorCode) {};
};

/**
 * @brief Exception thrown when a disk media error occurs
 */
class MediaException : public DiskException {
public:
    explicit MediaException(const std::string& message)
        : DiskException(message) {}

    MediaException(const std::string& message, int errorCode)
        : DiskException(message, errorCode) {};
};

/**
 * @brief Exception thrown when a disk I/O operation times out
 */
class IoTimeoutException : public DiskException {
public:
    explicit IoTimeoutException(const std::string& message)
        : DiskException(message) {}

    IoTimeoutException(const std::string& message, int errorCode)
        : DiskException(message, errorCode) {};
};

/**
 * @brief Exception thrown when a disk sector is bad or unreadable
 */
class BadSectorException : public DiskException {
public:
    explicit BadSectorException(const std::string& message)
        : DiskException(message) {}

    BadSectorException(const std::string& message, int errorCode)
        : DiskException(message, errorCode) {};
};

} // namespace disk
} // namespace recoverysuite