#ifndef RECOVERYSUITE_APPLICATION_SERVICE_GUIRECOVERYSERVICE_H
#define RECOVERYSUITE_APPLICATION_SERVICE_GUIRECOVERYSERVICE_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <functional>
#include <chrono>
#include "../Models/GUIModels.h"
#include "../RecoveryService.h"

namespace recoverysuite {
namespace application {
namespace service {

/**
 * @brief GUIRecoveryService
 *
 * A specialized service layer designed for GUI consumption.
 * This service provides a clean, stable interface for GUI applications
 * without exposing backend implementation details.
 *
 * This service wraps the existing RecoveryService and translates
 * its results into GUI-friendly models.
 */
class GUIRecoveryService {
public:
    /**
     * @brief Constructor
     * @param diskReader The disk reader to use for accessing storage devices
     * @param safetyPolicy Optional safety policy to apply (defaults to strict policy)
     */
    GUIRecoveryService(std::shared_ptr<recoverysuite::disk::IDiskReader> diskReader,
                       const recoverysuite::recovery::RecoverySafetyPolicy& safetyPolicy = recoverysuite::recovery::RecoverySafetyPolicy());

    /**
     * @brief Destructor
     */
    ~GUIRecoveryService();

    /**
     * @brief Enumerate all available storage devices
     * @return Vector of StorageDevice models representing available disks
     */
    std::vector<models::StorageDevice> enumerateStorageDevices();

    /**
     * @brief Get detailed information for a specific storage device
     * @param devicePath Path to the storage device (e.g., "/dev/sda")
     * @return StorageDevice model with detailed information
     */
    models::StorageDevice getStorageDeviceInfo(const std::string& devicePath);

    /**
     * @brief Get partitions for a specific storage device
     * @param devicePath Path to the storage device (e.g., "/dev/sda")
     * @return Vector of Partition models representing partitions on the device
     */
    std::vector<models::Partition> getDevicePartitions(const std::string& devicePath);

    /**
     * @brief Analyze the filesystem on a disk or disk range
     * @param devicePath Path to the storage device (e.g., "/dev/sda1")
     * @param startSector Starting sector to analyze (0-based, relative to device start)
     * @param numSectors Number of sectors to analyze
     * @return AnalysisResult containing analysis data or error information
     */
    models::AnalysisResult analyzeFilesystem(const std::string& devicePath, uint64_t startSector, uint64_t numSectors);

    /**
     * @brief Recover files from a filesystem
     * @param devicePath Path to the storage device (e.g., "/dev/sda1")
     * @param startSector Starting sector of the filesystem (0-based, relative to device start)
     * @param numSectors Number of sectors in the filesystem
     * @param outputPath Path where recovered files should be saved
     * @param progressCallback Optional callback for progress updates
     * @param cancellationToken Optional token to check for cancellation
     * @return RecoveryResult containing recovered files information or error information
     */
    models::RecoveryResult recoverFiles(const std::string& devicePath, uint64_t startSector, uint64_t numSectors,
                                       const std::string& outputPath,
                                       std::function<void(const models::RecoveryProgress&)> progressCallback = nullptr,
                                       std::function<bool()> cancellationToken = nullptr);

    /**
     * @brief Recover filesystem metadata
     * @param devicePath Path to the storage device (e.g., "/dev/sda1")
     * @param startSector Starting sector of the filesystem (0-based, relative to device start)
     * @param numSectors Number of sectors in the filesystem
     * @param outputPath Path where recovered metadata should be saved
     * @param progressCallback Optional callback for progress updates
     * @param cancellationToken Optional token to check for cancellation
     * @return RecoveryResult containing recovered metadata information or error information
     */
    models::RecoveryResult recoverMetadata(const std::string& devicePath, uint64_t startSector, uint64_t numSectors,
                                          const std::string& outputPath,
                                          std::function<void(const models::RecoveryProgress&)> progressCallback = nullptr,
                                          std::function<bool()> cancellationToken = nullptr);

    /**
     * @brief Carve files from raw disk data (file carving)
     * @param devicePath Path to the storage device (e.g., "/dev/sda")
     * @param startSector Starting sector to scan (0-based, relative to device start)
     * @param numSectors Number of sectors to scan
     * @param outputPath Path where carved files should be saved
     * @param progressCallback Optional callback for progress updates
     * @param cancellationToken Optional token to check for cancellation
     * @return RecoveryResult containing carved files information or error information
     */
    models::RecoveryResult carveFiles(const std::string& devicePath, uint64_t startSector, uint64_t numSectors,
                                     const std::string& outputPath,
                                     std::function<void(const models::RecoveryProgress&)> progressCallback = nullptr,
                                     std::function<bool()> cancellationToken = nullptr);

    /**
     * @brief Check if a sector range is valid on the disk
     * @param devicePath Path to the storage device (e.g., "/dev/sda")
     * @param startSector Starting sector to check (0-based, relative to device start)
     * @param numSectors Number of sectors to check
     * @return true if the sector range is valid, false otherwise
     */
    bool isSectorRangeValid(const std::string& devicePath, uint64_t startSector, uint64_t numSectors) const;

    /**
     * @brief Get disk information for a specific device
     * @param devicePath Path to the storage device (e.g., "/dev/sda")
     * @return DeviceInformation structure containing disk details
     */
    recoverysuite::disk::DeviceInformation getDiskInfo(const std::string& devicePath) const;

private:
    // The underlying recovery service
    std::unique_ptr<RecoveryService> recoveryService_;

    // Map to track ongoing operations for progress/cancellation
    std::map<std::string, std::function<bool()>> operationCancellationTokens_;

    // Helper to convert device path to disk number
    uint64_t extractDiskNumber(const std::string& devicePath) const;

    // Helper to get the underlying PhysicalDisk for a device path
    std::shared_ptr<recoverysuite::disk::IDiskReader> getDiskReaderForDevice(const std::string& devicePath) const;
};

} // namespace service
} // namespace application
} // namespace recoverysuite

#endif // RECOVERYSUITE_APPLICATION_SERVICE_GUIRECOVERYSERVICE_H