#include "RecoverySuite/Partition/GPTPartitionEntry.hpp"
#include <cstring>
#include <algorithm>
#include <RecoverySuite/Partition/PartitionException.hpp>

namespace recoverysuite {
namespace partition {

GPTPartitionEntry::GPTPartitionEntry(const std::array<std::byte, SIZE>& data)
    : m_data(data)
{
}

bool GPTPartitionEntry::isEmpty() const noexcept {
    // Check if all bytes are zero
    for (const auto& b : m_data) {
        if (b != std::byte{0}) {
            return false;
        }
    }
    return true;
}

std::array<uint8_t, 16> GPTPartitionEntry::getPartitionTypeGUID() const noexcept {
    std::array<uint8_t, 16> guid{};
    // Offset 0x00 in the entry (16 bytes)
    std::memcpy(guid.data(), m_data.data(), 16);
    return guid;
}

std::array<uint8_t, 16> GPTPartitionEntry::getUniquePartitionGUID() const noexcept {
    std::array<uint8_t, 16> guid{};
    // Offset 0x10 in the entry (16 bytes)
    std::memcpy(guid.data(), m_data.data() + 16, 16);
    return guid;
}

uint64_t GPTPartitionEntry::getStartingLBA() const noexcept {
    // Offset 0x20 in the entry (8 bytes, little-endian)
    uint64_t lba = 0;
    std::memcpy(&lba, m_data.data() + 0x20, 8);
    // Convert from little-endian to host (assuming host is little-endian, but we'll just return as is)
    // Since we are on a little-endian system, we can return directly.
    // However, to be explicit, we can avoid byte swap if we know the system is little-endian.
    // We'll assume little-endian.
    return lba;
}

uint64_t GPTPartitionEntry::getEndingLBA() const noexcept {
    // Offset 0x28 in the entry (8 bytes, little-endian)
    uint64_t lba = 0;
    std::memcpy(&lba, m_data.data() + 0x28, 8);
    return lba;
}

uint64_t GPTPartitionEntry::getAttributes() const noexcept {
    // Offset 0x30 in the entry (8 bytes, little-endian)
    uint64_t attr = 0;
    std::memcpy(&attr, m_data.data() + 0x30, 8);
    return attr;
}

std::u16string GPTPartitionEntry::getName() const noexcept {
    // Offset 0x38 in the entry: 72 UTF-16 characters (144 bytes)
    std::u16string name;
    name.resize(72);
    std::memcpy(name.data(), m_data.data() + 0x38, 72 * sizeof(char16_t));
    // Remove trailing null characters (if any)
    auto it = std::find(name.begin(), name.end(), static_cast<char16_t>(0));
    if (it != name.end()) {
        name.erase(it, name.end());
    }
    return name;
}

} // namespace partition
} // namespace recoverysuite