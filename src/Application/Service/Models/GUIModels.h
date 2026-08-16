#ifndef RECOVERYSUITE_APPLICATION_SERVICE_MODELS_GUIMODELS_H
#define RECOVERYSUITE_APPLICATION_SERVICE_MODELS_GUIMODELS_H

#include <string>
#include <cstdint>
#include <chrono>
#include <map>

namespace recoverysuite {
namespace application {
namespace service {
namespace models {

/**
 * @brief StorageDevice
 *
 * Model representing a storage device.
 */
struct StorageDevice {
    std::string devicePath;
    std::string modelNumber;
    std::string serialNumber;
    uint64_t totalBytes;
    uint32_t bytesPerSector;
    bool isReadOnly;
};

/**
 * @brief Partition
 *
 * Model representing a partition on a storage device.
 */
struct Partition {
    std::string partitionPath;
    uint64_t startSector;
    uint64_t numSectors;
    std::string filesystemType;
    bool isBootable;
};

/**
 * @brief FilesystemInfo
 *
 * Detailed information about a filesystem.
 */
struct FilesystemInfo {
    std::string type;
    std::string version;
    uint64_t clusterSizeBytes;
    std::string volumeLabel;
    std::string serialNumber;
    uint64_t totalSizeBytes;
    uint64_t usedSizeBytes;
    uint64_t freeSizeBytes;
    bool isReadOnly;
    bool isCorrupted;
};

/**
 * @brief AnalysisResult
 *
 * Result of a filesystem analysis operation.
 */
struct AnalysisResult {
    bool success;
    std::string errorMessage;
    FilesystemInfo filesystemInfo;
};

/**
 * @brief RecoveryProgress
 *
 * Progress information for a recovery operation.
 */
struct RecoveryProgress {
    std::string operationId;
    std::string operationType;
    std::string status;
    uint32_t percentage;
    std::string currentStep;
    uint64_t processedBytes;
    uint64_t totalBytes;
    uint64_t elapsedSeconds;
    uint64_t estimatedRemainingSeconds;
    uint64_t recoveredItemsCount;
    uint64_t failedItemsCount;
    bool isCancellable;
    bool isPausable;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief RecoveryResult
 *
 * Result of a recovery operation.
 */
struct RecoveryResult {
    bool success;
    std::string errorMessage;
    std::string operationType;
    uint64_t processedBytes;
    uint64_t recoveredBytes;
    uint64_t failedBytes;
    uint64_t recoveredItemsCount;
    uint64_t failedItemsCount;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::map<std::string, std::string> details;
};

/**
 * @brief RecoveryOperation
 *
 * Configuration for a recovery operation.
 */
struct RecoveryOperation {
    std::string operationType;
    std::string targetDevicePath;
    uint64_t startSector;
    uint64_t numSectors;
    std::string outputPath;
    bool verifyAfterRecovery;
    bool createLogFile;
};

} // namespace models
} // namespace service
} // namespace application
} // namespace recoverysuite


#endif // RECOVERYSUITE_APPLICATION_SERVICE_MODELS_GUIMODELS_H