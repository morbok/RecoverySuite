#ifndef RECOVERYSUITE_APPLICATION_SERVICE_MODELS_GUI_MODELS_H
#define RECOVERYSUITE_APPLICATION_SERVICE_MODELS_GUI_MODELS_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <memory>
#include <chrono>

namespace recoverysuite {
namespace application {
namespace service {
namespace models {

/**
 * @brief GUI-facing model for storage device information
 *
 * This model provides a clean abstraction for the GUI to consume
 * storage device information without depending on backend internals.
 */
struct StorageDevice {
    std::string devicePath;          // e.g., "/dev/sda" or "C:"
    std::string modelName;           // Disk model identifier
    std::string serialNumber;        // Disk serial number
    uint64_t totalSizeBytes;         // Total size in bytes
    uint32_t sectorSizeBytes;        // Sector size in bytes
    bool isRemovable;                // Whether the device is removable
    bool isReadOnly;                 // Whether the device is read-only

    StorageDevice() :
        totalSizeBytes(0),
        sectorSizeBytes(512),
        isRemovable(false),
        isReadOnly(false) {}
};

/**
 * @brief GUI-facing model for partition information
 *
 * This model provides a clean abstraction for the GUI to consume
 * partition information without depending on backend internals.
 */
struct Partition {
    std::string devicePath;          // e.g., "/dev/sda1"
    uint64_t startSector;            // Starting sector (0-based)
    uint64_t sizeSectors;            // Size in sectors
    std::string filesystemType;      // Detected filesystem type (e.g., "NTFS", "FAT32", "EXT4")
    std::string volumeLabel;         // Volume label if present
    bool isBootable;                 // Whether partition is bootable
    bool isRecognized;               // Whether filesystem is recognized
    uint64_t usedBytes;              // Used space in bytes
    uint64_t freeBytes;              // Free space in bytes

    Partition() :
        startSector(0),
        sizeSectors(0),
        isBootable(false),
        isRecognized(false),
        usedBytes(0),
        freeBytes(0) {}
};

/**
 * @brief GUI-facing model for filesystem information
 *
 * This model provides a clean abstraction for the GUI to consume
 * filesystem information without depending on backend internals.
 */
struct FilesystemInfo {
    std::string type;                // Filesystem type (e.g., "NTFS", "FAT32", "EXT4")
    std::string version;             // Filesystem version
    uint64_t totalSizeBytes;         // Total size in bytes
    uint64_t usedSizeBytes;          // Used size in bytes
    uint64_t freeSizeBytes;          // Free size in bytes
    uint32_t clusterSizeBytes;       // Cluster/allocation unit size
    uint64_t totalClusters;          // Total number of clusters
    uint64_t freeClusters;           // Number of free clusters
    std::string volumeLabel;         // Volume label
    std::string serialNumber;        // Volume serial number
    bool isReadOnly;                 // Whether filesystem is read-only
    bool isCorrupted;                // Whether filesystem appears corrupted

    FilesystemInfo() :
        totalSizeBytes(0),
        usedSizeBytes(0),
        freeSizeBytes(0),
        clusterSizeBytes(0),
        totalClusters(0),
        freeClusters(0),
        isReadOnly(false),
        isCorrupted(false) {}
};

/**
 * @brief GUI-facing model for filesystem analysis results
 *
 * This model provides a clean abstraction for the GUI to consume
 * filesystem analysis results without depending on backend internals.
 */
struct AnalysisResult {
    bool success;                    // Whether analysis succeeded
    std::string errorMessage;        // Error message if failed
    FilesystemInfo filesystemInfo;   // Basic filesystem information
    std::map<std::string, std::string> detailedAnalysis; // Detailed analysis results
    std::chrono::system_clock::time_point timestamp; // When analysis was performed

    AnalysisResult() : success(false) {}
};

/**
 * @brief GUI-facing model for recovery operation parameters
 *
 * This model provides a clean abstraction for the GUI to specify
 * recovery operations without depending on backend internals.
 */
struct RecoveryOperation {
    std::string operationType;       // Type of operation (e.g., "file_recovery", "metadata_recovery", "carving")
    std::string targetDevicePath;    // Target device path (e.g., "/dev/sda1")
    uint64_t startSector;            // Starting sector (0-based)
    uint64_t numSectors;             // Number of sectors to process
    std::string outputPath;          // Output path for recovered data
    bool verifyAfterRecovery;        // Whether to verify after recovery
    bool createLogFile;              // Whether to create detailed log file

    RecoveryOperation() :
        startSector(0),
        numSectors(0),
        verifyAfterRecovery(false),
        createLogFile(true) {}
};

/**
 * @brief GUI-facing model for recovery progress information
 *
 * This model provides a clean abstraction for the GUI to display
 * recovery progress without depending on backend internals.
 */
struct RecoveryProgress {
    std::string operationId;         // Unique identifier for the operation
    std::string operationType;       // Type of operation (e.g., "file_recovery")
    std::string status;              // Current status (e.g., "scanning", "recovering", "completed")
    uint32_t percentage;             // Progress percentage (0-100)
    std::string currentStep;         // Current step description
    uint64_t processedBytes;         // Number of bytes processed
    uint64_t totalBytes;             // Total bytes to process
    uint64_t elapsedSeconds;         // Elapsed time in seconds
    uint64_t estimatedRemainingSeconds; // Estimated remaining time in seconds
    uint64_t recoveredItemsCount;    // Number of items recovered so far
    uint64_t failedItemsCount;       // Number of items that failed
    bool isCancellable;              // Whether operation can be cancelled
    bool isPausable;                 // Whether operation can be paused
    std::chrono::system_clock::time_point timestamp; // When progress was last updated

    RecoveryProgress() :
        percentage(0),
        processedBytes(0),
        totalBytes(0),
        elapsedSeconds(0),
        estimatedRemainingSeconds(0),
        recoveredItemsCount(0),
        failedItemsCount(0),
        isCancellable(true),
        isPausable(false) {}
};

/**
 * @brief GUI-facing model for recovery operation results
 *
 * This model provides a clean abstraction for the GUI to display
 * recovery results without depending on backend internals.
 */
struct RecoveryResult {
    bool success;                    // Whether operation succeeded
    std::string errorMessage;        // Error message if failed
    std::string operationType;       // Type of operation performed
    uint64_t processedBytes;         // Total bytes processed
    uint64_t recoveredBytes;         // Total bytes successfully recovered
    uint64_t failedBytes;            // Total bytes that failed to recover
    uint64_t recoveredItemsCount;    // Number of items successfully recovered
    uint64_t failedItemsCount;       // Number of items that failed to recover
    std::chrono::system_clock::time_point startTime; // When operation started
    std::chrono::system_clock::time_point endTime;   // When operation ended
    std::map<std::string, std::string> details;      // Additional details

    RecoveryResult() :
        success(false),
        processedBytes(0),
        recoveredBytes(0),
        failedBytes(0),
        recoveredItemsCount(0),
        failedItemsCount(0) {}
};

/**
 * @brief GUI-facing model for a recovered item/file
 *
 * This model provides a clean abstraction for the GUI to display
 * recovered files/items without depending on backend internals.
 */
struct RecoveredItem {
    std::string fileName;            // Name of the recovered file
    std::string originalPath;        // Original path if available
    uint64_t fileSize;               // Size of the file in bytes
    std::string fileType;            // Detected file type (e.g., "JPEG", "PDF", "DOCX")
    uint64_t startSector;            // Starting sector where file was found
    uint64_t endSector;              // Ending sector where file was found
    bool isValid;                    // Whether file appears to be valid
    std::string validationInfo;      // Information about validation
    std::chrono::system_clock::time_point dateCreated; // File creation date if available
    std::chrono::system_clock::time_point dateModified; // File modification date if available

    RecoveredItem() :
        fileSize(0),
        startSector(0),
        endSector(0),
        isValid(false) {}
};

/**
 * @brief GUI-facing model for error information
 *
 * This model provides a clean abstraction for the GUI to display
 * error information without depending on backend internals.
 */
struct ErrorInfo {
    std::string errorCode;           // Unique error code
    std::string message;             // Human-readable error message
    std::string detailedMessage;     // Detailed technical error message
    std::string recoverySuggestion;  // Suggested recovery action
    bool isRecoverable;              // Whether error can be recovered from
    std::chrono::system_clock::time_point timestamp; // When error occurred

    ErrorInfo() : isRecoverable(false) {}
};

/**
 * @brief GUI-facing model for diagnostic/log events
 *
 * This model provides a clean abstraction for the GUI to display
 * diagnostic information without depending on backend internals.
 */
struct DiagnosticEvent {
    std::string eventId;             // Unique event identifier
    std::string category;            // Event category (e.g., "info", "warning", "error")
    std::string message;             // Event message
    std::string component;           // Component that generated the event
    std::chrono::system_clock::time_point timestamp; // When event occurred
    std::map<std::string, std::string> details;      // Additional event details

    DiagnosticEvent() {}
};

} // namespace models
} // namespace service
} // namespace application
} // namespace recoverysuite

#endif // RECOVERYSUITE_APPLICATION_SERVICE_MODELS_GUI_MODELS_H