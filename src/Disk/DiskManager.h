#pragma once

#include <memory>
#include <vector>
#include <string>
#include "DeviceInformation.h"
#include "Access/PhysicalDisk.h"
#include "DiskExceptions.h"
#include "Core/Version.h"

namespace recoverysuite {
namespace disk {

/**
 * @brief High-level manager for disk operations
 *
 * The DiskManager provides a simplified interface for common disk operations
 * and manages the lifecycle of PhysicalDisk objects.
 */
class DiskManager {
public:
    DiskManager();
    ~DiskManager();

    // Disable copy/move to prevent accidental duplication of resources
    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;
    DiskManager(DiskManager&&) = delete;
    DiskManager& operator=(DiskManager&&) = delete;

    /**
     * @brief Enumerate all available disk devices
     *
     * @return Vector of DeviceInformation for each detected disk
     * @throws DiskException if enumeration fails
     */
    std::vector<DeviceInformation> enumerateDisks();

    /**
     * @brief Get information for a specific disk by number
     *
     * @param diskNumber The physical disk number to query
     * @return DeviceInformation for the specified disk
     * @throws DiskNotFoundException if disk is not found
     * @throws DiskException if query fails
     */
    DeviceInformation getDiskInfo(uint64_t diskNumber);

    /**
     * @brief Open a physical disk for access
     *
     * @param diskNumber The physical disk number to open
     * @param readOnly Whether to open the disk in read-only mode
     * @return Unique pointer to a PhysicalDisk object
     * @throws AccessDeniedException if access is denied
     * @throws DiskNotFoundException if disk is not found
     * @throws DiskException if opening fails
     */
    std::unique_ptr<PhysicalDisk> openDisk(uint64_t diskNumber, bool readOnly = true);

    /**
     * @brief Get the number of disks detected
     *
     * @return Number of disks detected
     * @throws DiskException if query fails
     */
    uint64_t getDiskCount();

private:
    class DiskManagerImpl;
    std::unique_ptr<DiskManagerImpl> pImpl;
};

} // namespace disk
} // namespace recoverysuite