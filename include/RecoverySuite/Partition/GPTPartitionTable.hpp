#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include "RecoverySuite/Partition/GPTPartitionEntry.hpp"

namespace recoverysuite {
namespace partition {

/**
 * @brief GPT Partition Table
 *
 * Represents the array of partition entries read from disk.
 */
class GPTPartitionTable {
public:
    GPTPartitionTable() = default;
    explicit GPTPartitionTable(std::vector<GPTPartitionEntry> entries);

    size_t getPartitionCount() const noexcept;
    const GPTPartitionEntry& getEntry(size_t index) const;
    std::vector<GPTPartitionEntry>::const_iterator begin() const noexcept;
    std::vector<GPTPartitionEntry>::const_iterator end() const noexcept;

private:
    std::vector<GPTPartitionEntry> m_entries{};
};

} // namespace partition
} // namespace recoverysuite