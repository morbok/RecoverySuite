#include "RecoverySuite/Partition/GPTPartitionTable.hpp"

namespace recoverysuite {
namespace partition {

GPTPartitionTable::GPTPartitionTable(std::vector<GPTPartitionEntry> entries)
    : m_entries(std::move(entries))
{
}

size_t GPTPartitionTable::getPartitionCount() const noexcept {
    return m_entries.size();
}

const GPTPartitionEntry& GPTPartitionTable::getEntry(size_t index) const {
    return m_entries.at(index);
}

std::vector<GPTPartitionEntry>::const_iterator GPTPartitionTable::begin() const noexcept {
    return m_entries.begin();
}

std::vector<GPTPartitionEntry>::const_iterator GPTPartitionTable::end() const noexcept {
    return m_entries.end();
}

} // namespace partition
} // namespace recoverysuite