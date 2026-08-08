#pragma once

#include <cstdint>
#include <RecoverySuite/Partition/PartitionException.hpp>

namespace recoverysuite {
namespace partition {

/**
 * @brief Immutable partition geometry model
 *
 * Represents the logical geometry of a partition as parsed from MBR.
 */
class PartitionGeometry {
public:
    PartitionGeometry() = default;
    PartitionGeometry(
        uint32_t startLBA,
        uint32_t sectorCount,
        uint8_t partitionType,
        bool isBootable = false)
        : m_startLBA(startLBA)
        , m_sectorCount(sectorCount)
        , m_partitionType(partitionType)
        , m_isBootable(isBootable) {}

    /** Get starting LBA (Logical Block Address) */
    uint32_t getStartLBA() const noexcept { return m_startLBA; }

    /** Get sector count (number of sectors in partition) */
    uint32_t getSectorCount() const noexcept { return m_sectorCount; }

    /** Get partition type (e.g., 0x07 = NTFS, 0x0B = FAT32) */
    uint8_t getPartitionType() const noexcept { return m_partitionType; }

    /** Check if partition is bootable/active */
    bool isBootable() const noexcept { return m_isBootable; }

    /** Calculate ending LBA (inclusive) */
    uint32_t getEndLBA() const noexcept {
        if (m_sectorCount == 0) return 0;
        return m_startLBA + m_sectorCount - 1;
    }

    /** Check if partition geometry is valid */
    bool isValid() const noexcept {
        return m_sectorCount > 0 && m_partitionType != 0;
    }

private:
    uint32_t m_startLBA{0};      // Starting LBA
    uint32_t m_sectorCount{0};   // Number of sectors
    uint8_t m_partitionType{0};  // Partition type
    bool m_isBootable{false};    // Bootable/active flag
};

} // namespace partition
} // namespace recoverysuite