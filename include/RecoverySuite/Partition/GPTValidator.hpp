#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include "RecoverySuite/Partition/GPTHeader.hpp"
#include "RecoverySuite/Partition/GPTPartitionTable.hpp"
#include "RecoverySuite/Partition/PartitionException.hpp"
#include "IDiskReader.hpp"

namespace recoverysuite {
namespace partition {

/**
 * @brief Exception thrown when GPT signature is invalid
 */
class InvalidGPTSignatureException : public PartitionException {
public:
    explicit InvalidGPTSignatureException(const std::string& message)
        : PartitionException(message) {}
};

/**
 * @brief Exception thrown when GPT header CRC is invalid
 */
class InvalidGPTCRCException : public PartitionException {
public:
    explicit InvalidGPTCRCException(const std::string& message)
        : PartitionException(message) {}
};

/**
 * @brief Exception thrown when GPT partition entry array CRC is invalid
 */
class InvalidGPTEntryArrayCRCException : public PartitionException {
public:
    explicit InvalidGPTEntryArrayCRCException(const std::string& message)
        : PartitionException(message) {}
};

/**
 * @brief Exception thrown when GPT LBA ranges are invalid
 */
class InvalidGPTLBAException : public PartitionException {
public:
    explicit InvalidGPTLBAException(const std::string& message)
        : PartitionException(message) {}
};

/**
 * @brief Exception thrown when GPT partitions overlap
 */
class OverlappingGPTPartitionsException : public PartitionException {
public:
    explicit OverlappingGPTPartitionsException(const std::string& message)
        : PartitionException(message) {}
};

/**
 * @brief Validator for GPT header and partition entry array
 */
class GPTValidator {
public:
    GPTValidator() = default;

    /**
     * @brief Validate the GPT header
     * @param header The GPT header to validate
     * @throws InvalidGPTSignatureException if signature is invalid
     * @throws InvalidGPTCRCException if header CRC is invalid
     */
    void validateHeader(const GPTHeader& header) const;

    /**
     * @brief Validate the GPT partition entry array
     * @param header The GPT header (provides entry array LBA, size, count)
     * @param entries The partition entry array data (raw bytes)
     * @throws InvalidGPTEntryArrayCRCException if entry array CRC is invalid
     * @throws InvalidGPTLBAException if LBA ranges are invalid
     * @throws OverlappingGPTPartitionsException if partitions overlap
     */
    void validateEntryArray(const GPTHeader& header,
                            const std::vector<std::byte>& entryArrayData) const;

public:
    /**
     * @brief Compute CRC32 of a buffer
     * @param data Pointer to data
     * @param length Length in bytes
     * @return CRC32 value
     */
    static uint32_t computeCRC32(const void* data, size_t length);
};

} // namespace partition
} // namespace recoverysuite