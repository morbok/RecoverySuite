#pragma once

#include "PartitionTable.hpp"
#include <RecoverySuite/Partition/PartitionException.hpp>
#include <vector>
#include <cstdint>

namespace recoverysuite {
namespace partition {

/**
 * @brief Partition validator interface
 *
 * Validates partition table entries for correctness and consistency.
 */
class PartitionValidator {
public:
    PartitionValidator() = default;
    virtual ~PartitionValidator() = default;

    /**
     * Validate partition table
     * @param table Partition table to validate
     * @throws OverlappingPartitionsException if partitions overlap
     * @throws InvalidPartitionException if partition parameters are invalid
     */
    virtual void validate(const PartitionTable& table) const;

protected:
    /**
     * Check if two partition ranges overlap
     * @param start1 Start LBA of first partition
     * @param end1 End LBA of first partition (inclusive)
     * @param start2 Start LBA of second partition
     * @param end2 End LBA of second partition (inclusive)
     * @return true if ranges overlap
     */
    static bool rangesOverlap(
        uint32_t start1, uint32_t end1,
        uint32_t start2, uint32_t end2) noexcept {
        return !(end1 < start2 || end2 < start1);
    }
};

} // namespace partition
} // namespace recoverysuite