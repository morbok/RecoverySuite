#include "RecoverySuite/Partition/MBRPartitionEntry.hpp"
#include <RecoverySuite/Partition/PartitionException.hpp>

namespace recoverysuite {
namespace partition {

MBRPartitionEntry::MBRPartitionEntry(const std::array<std::byte, SIZE>& data) {
    m_data = data;
}

bool MBRPartitionEntry::isEmpty() const noexcept {
    // Check if all bytes are zero
    for (const auto& byte : m_data) {
        if (byte != std::byte{0}) {
            return false;
        }
    }
    return true;
}

uint8_t MBRPartitionEntry::getBootIndicator() const noexcept {
    return static_cast<unsigned char>(m_data[0]);
}

uint8_t MBRPartitionEntry::getPartitionType() const noexcept {
    return static_cast<unsigned char>(m_data[4]);
}

uint32_t MBRPartitionEntry::getStartLBA() const noexcept {
    return combineBytesLE(
        m_data[8],  // byte 8 (LSB)
        m_data[9],  // byte 9
        m_data[10], // byte 10
        m_data[11]  // byte 11 (MSB)
    );
}

uint32_t MBRPartitionEntry::getSectorCount() const noexcept {
    return combineBytesLE(
        m_data[12], // byte 12 (LSB)
        m_data[13], // byte 13
        m_data[14], // byte 14
        m_data[15]  // byte 15 (MSB)
    );
}

bool MBRPartitionEntry::isBootable() const noexcept {
    return getBootIndicator() == 0x80;
}

// Helper function to combine two bytes into a 16-bit little-endian value
uint16_t MBRPartitionEntry::combineBytesLE(std::byte low, std::byte high) noexcept {
    return static_cast<uint16_t>(
        static_cast<unsigned char>(low) |
        (static_cast<unsigned char>(high) << 8)
    );
}

// Helper function to combine four bytes into a 32-bit little-endian value
uint32_t MBRPartitionEntry::combineBytesLE(
    std::byte b0, std::byte b1, std::byte b2, std::byte b3) noexcept {
    return static_cast<uint32_t>(
        static_cast<unsigned char>(b0) |
        (static_cast<unsigned char>(b1) << 8) |
        (static_cast<unsigned char>(b2) << 16) |
        (static_cast<unsigned char>(b3) << 24)
    );
}

} // namespace partition
} // namespace recoverysuite