#pragma once

#include <string>
#include <vector>
#include "StorageDeviceInfo.h"
#include "StorageExceptions.h"
#include "../../Core/Version.h"
#include "DiskManager.h"

namespace recoverysuite {
namespace storage {

/**
 * @brief Low-level storage access interface
 *
 * This class provides access to storage devices through the Disk layer
 * and translates Disk layer information to Storage layer formats.
 */
class StorageAccess {
public:
    StorageAccess();
    ~StorageAccess();

    // Disable copy/move
    StorageAccess(const StorageAccess&) = delete;
    StorageAccess& operator=(const StorageAccess&) = delete;

    /**
     * @brief Get basic information for a storage device
     *
     * @param diskNumber The physical disk number to query
     * @return StorageDeviceInfo containing basic device information
     * @throws StorageException if query fails
     */
    StorageDeviceInfo getDeviceInfo(uint64_t diskNumber);

    /**
     * @brief Get health/SMART data for a storage device
     *
     * @param diskNumber The physical disk number to query
     * @return StorageHealthData containing health information
     * @throws StorageException if query fails
     */
    StorageHealthData getHealthData(uint64_t diskNumber);

    /**
     * @brief Check if TRIM is enabled on a storage device
     *
     * @param diskNumber The physical disk number to check
     * @return TRIMStatus indicating TRIM status
     * @throws StorageException if check fails
     */
    TRIMStatus getTrimStatus(uint64_t diskNumber);

    /**
     * @brief Get wear leveling assessment for a storage device
     *
     * @param diskNumber The physical disk number to assess
     * @return WearLevelingQuality indicating wear leveling quality
     * @throws StorageException if assessment fails
     */
    WearLevelingQuality getWearLevelingQuality(uint64_t diskNumber);

    /**
     * @brief Get garbage collection aggressiveness for a storage device
     *
     * @param diskNumber The physical disk number to assess
     * @return GarbageCollectionAggressiveness indicating GC aggressiveness
     * @throws StorageException if assessment fails
     */
    GarbageCollectionAggressiveness getGcAggressiveness(uint64_t diskNumber);

    /**
     * @brief Send a vendor-specific command to a storage device
     *
     * @param diskNumber The physical disk number
     * @param command The command to send
     * @param buffer Buffer for command response
     * @param bufferSize Size of the buffer
     * @return Number of bytes written to buffer
     * @throws StorageException if command fails
     */
    uint32_t sendVendorCommand(
        uint64_t diskNumber,
        const std::vector<uint8_t>& command,
        std::vector<uint8_t>& buffer);

private:
    class StorageAccessImpl;
    std::unique_ptr<StorageAccessImpl> pImpl;
};

} // namespace storage
} // namespace recoverysuite