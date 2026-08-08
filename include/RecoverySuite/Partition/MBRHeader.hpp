#pragma once

#include <array>
#include <cstdint>
#include <RecoverySuite/Partition/PartitionException.hpp>

namespace recoverysuite {
namespace partition {

/**
 * @brief Master Boot Record (MBR) header structure
 *
 * The MBR is located in the first sector (512 bytes) of a disk:
 * - Bytes 0-443: Boot code
 * - Bytes 444-509: Partition table (4 entries of 16 bytes each)
 * - Bytes 510-511: Boot signature (0x55AA)
 */
class MBRHeader {
public:
    /** Size of MBR boot code area */
    static constexpr size_t BOOT_CODE_SIZE = 444;
    /** Size of MBR partition table (4 entries * 16 bytes) */
    static constexpr size_t PARTITION_TABLE_SIZE = 64;
    /** Offset of boot signature in MBR */
    static constexpr size_t BOOT_SIGNATURE_OFFSET = 510;
    /** Expected boot signature value (0x55AA) */
    static constexpr uint16_t EXPECTED_BOOT_SIGNATURE = 0xAA55;
    /** Total size of MBR */
    static constexpr size_t SIZE = 512;

    MBRHeader() = default;
    explicit MBRHeader(const std::array<std::byte, SIZE>& data);

    /** Get the boot code area */
    const std::array<std::byte, BOOT_CODE_SIZE>& getBootCode() const noexcept {
        return m_bootCode;
    }

    /** Get the partition table area */
    const std::array<std::byte, PARTITION_TABLE_SIZE>& getPartitionTable() const noexcept {
        return m_partitionTable;
    }

    /** Check if MBR has valid boot signature (0x55AA) */
    bool hasValidSignature() const noexcept;

private:
    std::array<std::byte, BOOT_CODE_SIZE> m_bootCode{};
    std::array<std::byte, PARTITION_TABLE_SIZE> m_partitionTable{};

    /** Helper function to combine two bytes into a 16-bit little-endian value */
    static uint16_t combineBytesLE(std::byte low, std::byte high) noexcept;
};

} // namespace partition
} // namespace recoverysuite