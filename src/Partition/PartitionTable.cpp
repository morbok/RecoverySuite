#include "RecoverySuite/Partition/PartitionTable.hpp"
#include <cstring>
#include <stdexcept>
#include <RecoverySuite/Partition/PartitionException.hpp>

namespace recoverysuite {
namespace partition {

PartitionTable::PartitionTable(const std::array<std::byte, MBRHeader::PARTITION_TABLE_SIZE>& data) {
    // Initialize each of the 4 partition entries from the data array
    for (size_t i = 0; i < ENTRY_COUNT; ++i) {
        std::array<std::byte, ENTRY_SIZE> entryData{};
        // Copy 16 bytes for this entry
        std::memcpy(entryData.data(), data.data() + (i * ENTRY_SIZE), ENTRY_SIZE);
        m_entries[i] = MBRPartitionEntry(entryData);
    }
}

const MBRPartitionEntry& PartitionTable::getEntry(size_t index) const {
    if (index >= ENTRY_COUNT) {
        throw std::out_of_range("Partition entry index out of range");
    }
    return m_entries[index];
}

std::array<std::byte, PartitionTable::TABLE_SIZE> PartitionTable::getData() const noexcept {
    std::array<std::byte, TABLE_SIZE> result{};
    for (size_t i = 0; i < ENTRY_COUNT; ++i) {
        const auto& entry = m_entries[i];
        // Copy the raw entry data
        std::memcpy(result.data() + (i * ENTRY_SIZE), entry.getData().data(), ENTRY_SIZE);
    }
    return result;
}

} // namespace partition
} // namespace recoverysuite