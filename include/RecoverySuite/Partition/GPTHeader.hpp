#pragma once

#include <cstdint>
#include <array>
#include <string>

namespace recoverysuite {
namespace partition {

/**
 * @brief GPT Header structure (as defined by UEFI specification)
 *
 * This structure represents the GPT header, which is located at LBA 1
 * (and also at the last LBA for the backup header).
 */
struct GPTHeader {
    // Signature: "EFI PART" (8 bytes)
    std::array<char, 8> signature{};

    // Revision of the GPT header (little-endian)
    uint32_t revision{};

    // Size of the header in bytes (little-endian)
    uint32_t headerSize{};

    // CRC32 of the header (little-endian)
    uint32_t headerCRC32{};

    // Reserved (must be zero)
    uint32_t reserved{};

    // Current LBA of this header (little-endian)
    uint64_t myLBA{};

    // LBA of the backup GPT header (little-endian)
    uint64_t alternateLBA{};

    // First usable LBA for partitions (little-endian)
    uint64_t firstUsableLBA{};

    // Last usable LBA for partitions (little-endian)
    uint64_t lastUsableLBA{};

    // Disk GUID (16 bytes)
    std::array<uint8_t, 16> diskGUID{};

    // Starting LBA of the partition entry array (little-endian)
    uint64_t partitionEntryLBA{};

    // Number of partition entries in the array (little-endian)
    uint32_t numPartitionEntries{};

    // Size of each partition entry in bytes (little-endian)
    uint32_t sizeofPartitionEntry{};

    // CRC32 of the partition entry array (little-endian)
    uint32_t partitionEntryArrayCRC32{};

    // Reserved for the rest of the block (420 bytes to make header 512 bytes)
    std::array<char, 420> reserved2{};

    /**
     * @brief Check if the header has a valid signature
     * @return True if signature is "EFI PART"
     */
    bool hasValidSignature() const noexcept {
        return signature[0] == 'E' && signature[1] == 'F' && signature[2] == 'I' &&
               signature[3] == ' ' && signature[4] == 'P' && signature[5] == 'A' &&
               signature[6] == 'R' && signature[7] == 'T';
    }

    /**
     * @brief Check if the header is for a protective MBR (not directly in header, but we can note)
     * @note Protective MBR is indicated by the MBR, not the GPT header.
     *       This function is placeholder for clarity.
     * @return False (header alone doesn't indicate protective MBR)
     */
    bool isProtectiveMBR() const noexcept {
        return false; // Determined by MBR, not GPT header
    }
};

} // namespace partition
} // namespace recoverysuite