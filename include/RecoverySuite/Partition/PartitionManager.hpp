#pragma once

#include <memory>
#include <utility>
#include <vector>
#include "MBRHeader.hpp"
#include "PartitionTable.hpp"
#include "PartitionGeometry.hpp"
#include <IDiskReader.hpp>

namespace recoverysuite {
namespace partition {

/**
 * @brief High-level partition manager interface
 *
 * Provides convenient methods for reading and working with MBR partitions.
 */
class PartitionManager {
public:
    explicit PartitionManager(std::shared_ptr<recoverysuite::disk::IDiskReader> reader);
    virtual ~PartitionManager() = default;

    /**
     * Read MBR from disk
     * @return Pair of (MBRHeader, PartitionTable)
     * @throws PartitionException on read or parsing errors
     */
    virtual std::pair<MBRHeader, PartitionTable> readMBR() const;

    /**
     * Validate partitions on disk
     * @throws OverlappingPartitionsException if partitions overlap
     * @throws InvalidPartitionException if partition parameters are invalid
     */
    virtual void validatePartitions() const;

    /**
     * Get geometries of all defined partitions
     * @return Vector of partition geometries (only for non-empty entries)
     */
    virtual std::vector<PartitionGeometry> getPartitionGeometries() const;

protected:
    std::shared_ptr<recoverysuite::disk::IDiskReader> m_reader;
};

} // namespace partition
} // namespace recoverysuite