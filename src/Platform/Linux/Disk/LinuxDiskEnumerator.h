#pragma once

#include <vector>
#include <memory>
#include <string>
#include "../../IDiskEnumerator.h"
#include "../../../Core/Version.h"
#include "../../../Disk/DeviceInformation.h"
#include "../../../Disk/DiskExceptions.h"

namespace recoverysuite {
namespace platform {
namespace linux {

/**
 * @brief Linux-specific implementation of disk enumerator
 *
 * This is a simplified mock implementation for testing purposes.
 * In a real implementation, this would use sysfs, /proc, or other Linux-specific interfaces.
 */
class LinuxDiskEnumerator : public virtual IDiskEnumerator {
public:
    ~LinuxDiskEnumerator() override = default;

    /**
     * @brief Enumerate all available disk devices
     * @return Vector of DeviceInformation for each detected disk
     */
    std::vector<recoverysuite::disk::DeviceInformation> enumerateDisks() override {
        // Return a mock disk for testing
        std::vector<recoverysuite::disk::DeviceInformation> disks;
        recoverysuite::disk::DeviceInformation info;
        info.modelNumber = "Mock Linux Disk";
        info.serialNumber = "MOCKLINUX123";
        info.firmwareRevision = "1.0";
        info.bytesPerSector = 512;
        info.totalSectors = 1000;
        info.busType = recoverysuite::disk::BusType::SATA;
        disks.push_back(info);
        return disks;
    }

    /**
     * @brief Get information for a specific disk by number
     * @param diskNumber The physical disk number to query
     * @return DeviceInformation for the specified disk
     */
    recoverysuite::disk::DeviceInformation getDiskInfo(uint64_t diskNumber) override {
        // For simplicity, we'll return the same mock disk for any disk number
        // In a real implementation, we would look up the disk by number
        recoverysuite::disk::DeviceInformation info;
        info.modelNumber = "Mock Linux Disk";
        info.serialNumber = "MOCKLINUX123";
        info.firmwareRevision = "1.0";
        info.bytesPerSector = 512;
        info.totalSectors = 1000;
        info.busType = recoverysuite::disk::BusType::SATA;
        info.diskNumber = diskNumber;
        return info;
    }

    /**
     * @brief Get the number of disks detected
     * @return Number of disks detected
     */
    uint64_t getDiskCount() override {
        // Return 1 for our mock disk
        return 1;
    }
};

} // namespace linux
} // namespace platform
} // namespace recoverysuite