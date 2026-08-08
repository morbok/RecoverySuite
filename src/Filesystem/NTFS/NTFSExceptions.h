#ifndef RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_EXCEPTIONS_H
#define RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_EXCEPTIONS_H

#include <stdexcept>
#include <string>
#include "RecoverySuite/Errors/RecoverySuiteException.h"

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

class NTFSException : public RecoverySuiteException {
public:
    explicit NTFSException(const std::string& message, int errorCode = 0)
        : RecoverySuiteException("NTFS Error: " + message, errorCode) {}
    
    NTFSException(const std::string& message, const std::string& details, int errorCode = 0)
        : RecoverySuiteException("NTFS Error: " + message + " - " + details, errorCode) {}
};

// Specific NTFS exception types
class InvalidBootSectorException : public NTFSException {
public:
    explicit InvalidBootSectorException(const std::string& details = "")
        : NTFSException("Invalid NTFS boot sector", details, 0x80001000) {}
};

class CorruptedMFTException : public NTFSException {
public:
    explicit CorruptedMFTException(const std::string& details = "")
        : NTFSException("Corrupted MFT detected", details, 0x80001001) {}
};

class InvalidAttributeException : public NTFSException {
public:
    explicit InvalidAttributeException(const std::string& details = "")
        : NTFSException("Invalid NTFS attribute", details, 0x80001002) {}
};

class InvalidFileRecordException : public NTFSException {
public:
    explicit InvalidFileRecordException(const std::string& details = "")
        : NTFSException("Invalid NTFS file record", details, 0x80001003) {}
};

class NTFSNotSupportedException : public NTFSException {
public:
    explicit NTFSNotSupportedException(const std::string& feature = "")
        : NTFSException("NTFS feature not supported: " + feature, "", 0x80001004) {}
};

class NTFSIOException : public NTFSException {
public:
    explicit NTFSIOException(const std::string& details = "")
        : NTFSException("NTFS I/O error", details, 0x80001005) {}
};

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_EXCEPTIONS_H
