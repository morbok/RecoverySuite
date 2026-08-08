#pragma once

#include <cstdint>
#include "Core/Version.h"

namespace recoverysuite {
namespace disk {

/**
 * @brief Structure representing the drive geometry of a disk
 *
 * This structure contains the traditional CHS (Cylinder-Head-Sector) geometry
 * information for a disk drive. For modern disks, this information may be
 * logical rather than physical.
 */
struct DriveGeometry {
    uint32_t cylinders;       // Number of cylinders
    uint32_t heads;           // Number of heads
    uint32_t sectors_per_track; // Number of sectors per track
    uint32_t bytes_per_sector;  // Number of bytes per sector

    /**
     * @brief Calculate the total capacity based on CHS geometry
     *
     * @return Total capacity in bytes
     */
    uint64_t getCapacityBytes() const {
        return static_cast<uint64_t>(cylinders) *
               static_cast<uint64_t>(heads) *
               static_cast<uint64_t>(sectors_per_track) *
               static_cast<uint64_t>(bytes_per_sector);
    }
};

} // namespace disk
} // namespace recoverysuite