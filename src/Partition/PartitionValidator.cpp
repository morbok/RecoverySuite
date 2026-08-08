#include "RecoverySuite/Partition/PartitionValidator.hpp"
#include <RecoverySuite/Partition/PartitionException.hpp>
#include <stdexcept>
#include <system_error>
#include <climits>

namespace recoverysuite {
namespace partition {

void PartitionValidator::validate(const PartitionTable& table) const {
    // Check each partition entry for basic validity
    for (size_t i = 0; i < table.getPartitionCount(); ++i) {
        const auto& entry = table.getEntry(i);

        // Skip empty entries
        if (entry.isEmpty()) {
            continue;
        }

        // Validate partition type (should not be 0 for non-empty entries)
        if (entry.getPartitionType() == 0) {
            throw InvalidPartitionException(
                "Partition type cannot be zero for non-empty partition",
                static_cast<int>(std::errc::invalid_argument)
            );
        }

        // Validate LBA and sector count (basic sanity checks)
        // Note: More detailed validation (like checking against disk size)
        // should be done at a higher level with disk geometry information
    }

    // Check for overlapping partitions
    std::vector<std::pair<uint32_t, uint32_t>> ranges; // {startLBA, endLBA>

    for (size_t i = 0; i < table.getPartitionCount(); ++i) {
        const auto& entry = table.getEntry(i);

        // Skip empty entries
        if (entry.isEmpty()) {
            continue;
        }

        uint32_t startLBA = entry.getStartLBA();
        uint32_t sectorCount = entry.getSectorCount();

        // Check for invalid values
        if (sectorCount == 0) {
            throw InvalidPartitionException(
                "Partition sector count cannot be zero",
                static_cast<int>(std::errc::invalid_argument)
            );
        }

        // Check for potential overflow in end LBA calculation
        if (sectorCount > 0 && startLBA > UINT32_MAX - sectorCount + 1) {
            throw InvalidPartitionException(
                "Partition LBA range would overflow",
                static_cast<int>(std::errc::result_out_of_range)
            );
        }

        uint32_t endLBA = startLBA + sectorCount - 1;
        ranges.emplace_back(startLBA, endLBA);
    }

    // Check all pairs of partitions for overlap
    for (size_t i = 0; i < ranges.size(); ++i) {
        for (size_t j = i + 1; j < ranges.size(); ++j) {
            if (rangesOverlap(
                    ranges[i].first, ranges[i].second,
                    ranges[j].first, ranges[j].second)) {
                throw OverlappingPartitionsException(
                    "Partitions overlap",
                    static_cast<int>(std::errc::invalid_argument)
                );
            }
        }
    }
}

} // namespace partition
} // namespace recoverysuite