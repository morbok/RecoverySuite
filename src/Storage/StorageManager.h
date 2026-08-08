#pragma once

#include <memory>
#include <vector>
#include <string>
#include "StorageDeviceInfo.h"
#include "StorageExceptions.h"
#include "../Core/Version.h"  // TEMP
#include "DiskManager.h"

namespace recoverysuite {
namespace storage {

/**
 * @brief Forward declarations
 */
class StorageAccess;
class StorageReporter;
class StorageUtils;

/**
 * @brief High-level manager for storage analysis operations
 *
 * The StorageManager provides a simplified interface for storage analysis
 * operations and coordinates between different analysis components.
 */
class StorageManager {
public:
    StorageManager();
    explicit StorageManager(std::unique_ptr<StorageAccess> storageAccess);
    virtual ~StorageManager();

    // Disable copy/move to prevent accidental duplication of resources
    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;
    StorageManager(StorageManager&&) = delete;
    StorageManager& operator=(StorageManager&&) = delete;

    /**
     * @brief Analyze a storage device and generate a comprehensive report
     *
     * @param diskNumber The physical disk number to analyze
     * @return StorageAnalysisReport containing analysis results
     * @throws StorageException if analysis fails
     */
    virtual StorageAnalysisReport analyzeStorage(uint64_t diskNumber);

    /**
     * @brief Get basic information for a storage device
     *
     * @param diskNumber The physical disk number to query
     * @return StorageDeviceInfo containing basic device information
     * @throws StorageException if query fails
     */
    virtual StorageDeviceInfo getStorageInfo(uint64_t diskNumber);

    /**
     * @brief Check if TRIM is enabled on a storage device
     *
     * @param diskNumber The physical disk number to check
     * @return TRIMStatus indicating TRIM status
     * @throws StorageException if check fails
     */
    virtual TRIMStatus checkTrimStatus(uint64_t diskNumber);

    /**
     * @brief Get wear leveling assessment for a storage device
     *
     * @param diskNumber The physical disk number to assess
     * @return WearLevelingQuality indicating wear leveling quality
     * @throws StorageException if assessment fails
     */
    virtual WearLevelingQuality assessWearLeveling(uint64_t diskNumber);

    /**
     * @brief Get garbage collection aggressiveness for a storage device
     *
     * @param diskNumber The physical disk number to assess
     * @return GarbageCollectionAggressiveness indicating GC aggressiveness
     * @throws StorageException if assessment fails
     */
    virtual GarbageCollectionAggressiveness assessGcAggressiveness(uint64_t diskNumber);

private:
    class StorageManagerImpl;
    std::unique_ptr<StorageManagerImpl> pImpl;
};

} // namespace storage
} // namespace recoverysuite