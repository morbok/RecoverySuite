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

    // Copy boot signature (bytes 510-511)
    std::memcpy(m_bootSignature.data(), data.data() + BOOT_SIGNATURE_OFFSET, BOOT_SIGNATURE_SIZE);
}

bool MBRHeader::hasValidSignature() const noexcept {
    // Check boot signature bytes for 0x55AA (stored little-endian: low byte first)
    if (m_bootSignature.size() < BOOT_SIGNATURE_SIZE) return false;

    std::byte low = m_bootSignature[0];  // First byte of boot signature (offset 510)
    std::byte high = m_bootSignature[1]; // Second byte of boot signature (offset 511)

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