#pragma once

#include <array>
#include <cstdint>
#include <RecoverySuite/Partition/PartitionException.hpp>

namespace recoverysuite {
namespace partition {

/**
 * @brief MBR partition table entry (16 bytes)
 *
 * Structure of a partition table entry:
 * - Byte 0: Boot indicator (0x80 = active, 0x00 = inactive)
 * - Bytes 1-3: Starting CHS address (CHS is obsolete but maintained for compatibility)
 * - Byte 4: Partition type
 * - Bytes 5-7: Ending CHS address
 * - Bytes 8-11: Starting LBA (little endian)
 * - Bytes 12-15: Sector count (little endian)
 */
class MBRPartitionEntry {
public:
    static constexpr size_t SIZE = 16;

    MBRPartitionEntry() = default;
    explicit MBRPartitionEntry(const std::array<std::byte, SIZE>& data);

    /** Check if partition entry is empty (all zeros) */
    bool isEmpty() const noexcept;

    /** Get boot indicator (0x80 = active/bootable, 0x00 = inactive) */
    uint8_t getBootIndicator() const noexcept;

    /** Get partition type (e.g., 0x07 = NTFS, 0x0B = FAT32) */
    uint8_t getPartitionType() const noexcept;

    /** Get starting LBA (Logical Block Address) */
    uint32_t getStartLBA() const noexcept;

    /** Get sector count (number of sectors in partition) */
    uint32_t getSectorCount() const noexcept;

    /** Check if partition is bootable */
    bool isBootable() const noexcept;

    /** Get raw entry data */
    const std::array<std::byte, SIZE>& getData() const noexcept {
        return m_data;
    }

private:
    std::array<std::byte, SIZE> m_data{};

    /** Helper function to combine two bytes into a 16-bit little-endian value */
    static uint16_t combineBytesLE(std::byte low, std::byte high) noexcept;

    /** Helper function to combine four bytes into a 32-bit little-endian value */
    static uint32_t combineBytesLE(
        std::byte b0, std::byte b1, std::byte b2, std::byte b3) noexcept;
};

} // namespace partition
} // namespace recoverysuite