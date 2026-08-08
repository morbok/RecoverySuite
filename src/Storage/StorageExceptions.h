#pragma once

#include <stdexcept>
#include <string>
#include "../Core/Version.h"

namespace recoverysuite {
namespace storage {

/**
 * @brief Base exception class for all storage-related errors
 */
class StorageException : public std::runtime_error {
public:
    explicit StorageException(const std::string& message)
        : std::runtime_error(message) {}

    StorageException(const std::string& message, int errorCode)
        : std::runtime_error(message), m_errorCode(errorCode) {}

    int getErrorCode() const { return m_errorCode; }

private:
    int m_errorCode = 0;
};

/**
 * @brief Exception thrown when a storage operation fails due to invalid parameters
 */
class InvalidParameterException : public StorageException {
public:
    explicit InvalidParameterException(const std::string& message)
        : StorageException(message) {}

    InvalidParameterException(const std::string& message, int errorCode)
        : StorageException(message, errorCode) {}
};

/**
 * @brief Exception thrown when a storage device is not found or inaccessible
 */
class DeviceNotFoundException : public StorageException {
public:
    explicit DeviceNotFoundException(const std::string& message)
        : StorageException(message) {}

    DeviceNotFoundException(const std::string& message, int errorCode)
        : StorageException(message, errorCode) {}
};

/**
 * @brief Exception thrown when storage access is denied (often due to permissions)
 */
class AccessDeniedException : public StorageException {
public:
    explicit AccessDeniedException(const std::string& message)
        : StorageException(message) {}

    AccessDeniedException(const std::string& message, int errorCode)
        : StorageException(message, errorCode) {}
};

/**
 * @brief Exception thrown when a storage media error occurs
 */
class MediaException : public StorageException {
public:
    explicit MediaException(const std::string& message)
        : StorageException(message) {}

    MediaException(const std::string& message, int errorCode)
        : StorageException(message, errorCode) {}
};

/**
 * @brief Exception thrown when a storage I/O operation times out
 */
class IoTimeoutException : public StorageException {
public:
    explicit IoTimeoutException(const std::string& message)
        : StorageException(message) {}

    IoTimeoutException(const std::string& message, int errorCode)
        : StorageException(message, errorCode) {}
};

/**
 * @brief Exception thrown when a storage sector is bad or unreadable
 */
class BadSectorException : public StorageException {
public:
    explicit BadSectorException(const std::string& message)
        : StorageException(message) {}

    BadSectorException(const std::string& message, int errorCode)
        : StorageException(message, errorCode) {};
};

} // namespace storage
} // namespace recoverysuite