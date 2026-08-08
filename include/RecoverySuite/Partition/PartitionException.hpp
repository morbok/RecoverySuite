#pragma once

#include <stdexcept>
#include <string>
#include "Version.h"

namespace recoverysuite {
namespace partition {

/**
 * @brief Base exception class for all partition-related errors
 */
class PartitionException : public std::runtime_error {
public:
    explicit PartitionException(const std::string& message)
        : std::runtime_error(message) {}

    PartitionException(const std::string& message, int errorCode)
        : std::runtime_error(message), m_errorCode(errorCode) {}

    int getErrorCode() const { return m_errorCode; }

private:
    int m_errorCode = 0;
};

/**
 * @brief Exception thrown when an MBR signature is invalid
 */
class InvalidMBRSignatureException : public PartitionException {
public:
    explicit InvalidMBRSignatureException(const std::string& message)
        : PartitionException(message) {}

    InvalidMBRSignatureException(const std::string& message, int errorCode)
        : PartitionException(message, errorCode) {}
};

/**
 * @brief Exception thrown when partition entries overlap
 */
class OverlappingPartitionsException : public PartitionException {
public:
    explicit OverlappingPartitionsException(const std::string& message)
        : PartitionException(message) {}

    OverlappingPartitionsException(const std::string& message, int errorCode)
        : PartitionException(message, errorCode) {}
};

/**
 * @brief Exception thrown when partition parameters are invalid
 */
class InvalidPartitionException : public PartitionException {
public:
    explicit InvalidPartitionException(const std::string& message)
        : PartitionException(message) {}

    InvalidPartitionException(const std::string& message, int errorCode)
        : PartitionException(message, errorCode) {}
};

} // namespace partition
} // namespace recoverysuite