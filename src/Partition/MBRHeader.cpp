#include "RecoverySuite/Partition/MBRHeader.hpp"
#include <cstring>
#include <RecoverySuite/Partition/PartitionException.hpp>

namespace recoverysuite {
namespace partition {

MBRHeader::MBRHeader(const std::array<std::byte, SIZE>& data) {
    // Copy boot code (bytes 0-443)
    std::memcpy(m_bootCode.data(), data.data(), BOOT_CODE_SIZE);

    // Copy partition table (bytes 444-509)
    std::memcpy(m_partitionTable.data(), data.data() + BOOT_CODE_SIZE, PARTITION_TABLE_SIZE);
}

bool MBRHeader::hasValidSignature() const noexcept {
    // Check bytes 510-511 for boot signature 0x55AA (stored little-endian as 0xAA55)
    if (m_partitionTable.size() < 2) return false;

    // The boot signature is at the end of the partition table area
    // In the m_partitionTable array, bytes 510-511 correspond to indices 62-63
    // because m_partitionTable starts at offset 444 in the MBR
    std::byte low = m_partitionTable[62];  // Offset 510 in MBR = 444 + 62
    std::byte high = m_partitionTable[63]; // Offset 511 in MBR = 444 + 63

    uint16_t signature = combineBytesLE(low, high);
    return signature == EXPECTED_BOOT_SIGNATURE;
}

// Helper function to combine two bytes into a 16-bit little-endian value
uint16_t MBRHeader::combineBytesLE(std::byte low, std::byte high) noexcept {
    return static_cast<uint16_t>(
        static_cast<unsigned char>(low) |
        (static_cast<unsigned char>(high) << 8)
    );
}

} // namespace partition
} // namespace recoverysuite