#ifndef RECOVERYSUITE_FILESYSTEM_FILESYSTEM_EXCEPTIONS_H
#define RECOVERYSUITE_FILESYSTEM_FILESYSTEM_EXCEPTIONS_H

#include <stdexcept>
#include <string>
#include "Version.h"

namespace recoverysuite {
namespace filesystem {

/**
 * @brief Base exception class for all filesystem-related errors
 */
class FilesystemException : public std::runtime_error {
public:
    explicit FilesystemException(const std::string& message)
        : std::runtime_error("Filesystem Error: " + message), m_errorCode(0) {}

    FilesystemException(const std::string& message, int errorCode)
        : std::runtime_error("Filesystem Error: " + message), m_errorCode(errorCode) {}

    FilesystemException(const std::string& message, const std::string& details, int errorCode = 0)
        : std::runtime_error("Filesystem Error: " + message + " - " + details), m_errorCode(errorCode) {}

    int getErrorCode() const { return m_errorCode; }

private:
    int m_errorCode;
};

// Specific filesystem exception types
class InvalidFilesystemException : public FilesystemException {
public:
    explicit InvalidFilesystemException(const std::string& details = "")
        : FilesystemException("Invalid filesystem detected", details, 0x80002000) {}

    InvalidFilesystemException(const std::string& details, int errorCode)
        : FilesystemException("Invalid filesystem detected", details, errorCode) {};
};

class CorruptedFilesystemException : public FilesystemException {
public:
    explicit CorruptedFilesystemException(const std::string& details = "")
        : FilesystemException("Corrupted filesystem detected", details, 0x80002001) {}

    CorruptedFilesystemException(const std::string& details, int errorCode)
        : FilesystemException("Corrupted filesystem detected", details, errorCode) {};
};

class UnsupportedFilesystemException : public FilesystemException {
public:
    explicit UnsupportedFilesystemException(const std::string& filesystemType = "")
        : FilesystemException("Unsupported filesystem: " + filesystemType, "", 0x80002002) {}

    UnsupportedFilesystemException(const std::string& filesystemType, int errorCode)
        : FilesystemException("Unsupported filesystem: " + filesystemType, "", errorCode) {};
};

class FilesystemIOException : public FilesystemException {
public:
    explicit FilesystemIOException(const std::string& details = "")
        : FilesystemException("Filesystem I/O error", details, 0x80002003) {}

    FilesystemIOException(const std::string& details, int errorCode)
        : FilesystemException("Filesystem I/O error", details, errorCode) {};
};

} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FILESYSTEM_EXCEPTIONS_H