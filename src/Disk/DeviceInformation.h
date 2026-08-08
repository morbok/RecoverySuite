#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "../Core/Version.h"

namespace recoverysuite {
namespace disk {

/**
 * @brief Enumeration of bus types for disk devices
 */
enum class BusType {
    Unknown,
    SCSI,
    ATAPI,
    ATA,
    IEEE1394,
    SSA,
    Fibre,
    USB,
    RAID,
    iSCSI,
    SAS,
    SATA,
    SD
};

/**
 * @brief Enumeration of partition styles
 */
enum class PartitionStyle {
    Unknown,
    MBR,
    GPT
};

/**
 * @brief Structure containing detailed information about a disk device
 */
struct DeviceInformation {
    // Basic identification
    uint64_t diskNumber = 0;           // Physical disk number (0-based)
    bool isRemovable = false;          // Whether the disk is removable media
    bool isReadOnly = false;           // Whether the disk is read-only

    // Identity information
    std::string modelNumber;           // Model number of the disk
    std::string serialNumber;          // Serial number of the disk
    std::string firmwareRevision;      // Firmware revision

    // Bus and interface information
    BusType busType = BusType::Unknown;
    std::string busTypeString;         // Human-readable bus type

    // Capacity and geometry
    uint64_t totalSectors = 0;         // Total number of sectors on disk
    uint64_t bytesPerSector = 0;       // Bytes per sector (usually 512 or 4096)
    uint64_t totalSizeBytes = 0;       // Total size in bytes

    // Sector sizes (important for modern drives with advanced format)
    uint32_t logicalBytesPerSector = 0; // Logical sector size
    uint32_t physicalBytesPerSector = 0; // Physical sector size

    // Partition information
    PartitionStyle partitionStyle = PartitionStyle::Unknown;
    bool hasGptProtectiveMbr = false;  // Whether GPT disk has protective MBR

    // Additional capabilities
    bool supportsTrim = false;         // Whether disk supports TRIM
    bool supportsNcqt = false;         // Whether disk supports Non-Command Queued Trim
    bool supportsDsmt = false;         // Whether disk supports Deterministic Senser Data after TRIM

    // SMART support (placeholder for future implementation)
    bool smartSupported = false;       // Whether SMART is supported
    bool smartEnabled = false;         // Whether SMART is enabled

    // Constructor
    DeviceInformation() = default;

    // Clear all information
    void clear() {
        *this = DeviceInformation();
    }

    // Check if the device information is valid
    bool isValid() const {
        return diskNumber != 0 || !modelNumber.empty() || totalSizeBytes > 0;
    }
};



} // namespace disk
} // namespace recoverysuite