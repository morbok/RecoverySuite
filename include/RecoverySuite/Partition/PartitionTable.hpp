#pragma once

#include <array>
#include <cstdint>
#include "MBRHeader.hpp"
#include "MBRPartitionEntry.hpp"
#include <RecoverySuite/Partition/PartitionException.hpp>

namespace recoverysuite {
namespace partition {

/**
 * @brief MBR partition table wrapper
 *
 * Provides access to the 4 partition entries in the MBR partition table
 * and validates the partition table structure.
 */
class PartitionTable {
public:
    static constexpr size_t ENTRY_COUNT = 4;
    static constexpr size_t ENTRY_SIZE = 16; // bytes per partition entry
    static constexpr size_t TABLE_SIZE = ENTRY_COUNT * ENTRY_SIZE; // 64 bytes

    PartitionTable() = default;
    explicit PartitionTable(const std::array<std::byte, MBRHeader::PARTITION_TABLE_SIZE>& data);

    /** Get number of partition entries */
    size_t getPartitionCount() const noexcept { return ENTRY_COUNT; }

    /** Get partition entry by index */
    const MBRPartitionEntry& getEntry(size_t index) const;

    /** Get raw partition table data as bytes */
    std::array<std::byte, TABLE_SIZE> getData() const noexcept;

private:
    std::array<MBRPartitionEntry, ENTRY_COUNT> m_entries{};
};

} // namespace partition
} // namespace recoverysuite