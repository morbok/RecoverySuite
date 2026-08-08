#pragma once

#include <vector>
#include <memory>
#include "../Disk/DeviceInformation.h"
#include "DiskExceptions.h"
#include "../Core/Version.h"

namespace recoverysuite {
namespace platform {

/**
 * @brief Platform-independent disk enumerator interface
 *
 * This interface defines the contract for platform-specific disk enumerators.
 * Each platform (Windows, Linux, macOS) will provide its own implementation.
 */
class IDiskEnumerator {
public:
    virtual ~IDiskEnumerator() = default;

    /**
     * @brief Enumerate all available disk devices
     *
     * @return Vector of DeviceInformation for each detected disk
     * @throws DiskException if enumeration fails
     */
    virtual std::vector<recoverysuite::disk::DeviceInformation> enumerateDisks() = 0;

    /**
     * @brief Get information for a specific disk by number
     *
     * @param diskNumber The physical disk number to query
     * @return DeviceInformation for the specified disk
     * @throws DiskNotFoundException if disk is not found
     * @throws DiskException if query fails
     */
    virtual recoverysuite::disk::DeviceInformation getDiskInfo(uint64_t diskNumber) = 0;

    /**
     * @brief Get the number of disks detected
     *
     * @return Number of disks detected
     * @throws DiskException if query fails
     */
    virtual uint64_t getDiskCount() = 0;
};

} // namespace platform
} // namespace recoverysuite