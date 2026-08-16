#include "GUIRecoveryService.h"
#include "RecoveryService.h"
#include "../../Logging/Logger.h"
#include <stdexcept>
#include <chrono>
#include <thread>

namespace recoverysuite {
namespace application {
namespace service {

GUIRecoveryService::GUIRecoveryService(std::shared_ptr<recoverysuite::disk::IDiskReader> diskReader,
                                       const recoverysuite::recovery::RecoverySafetyPolicy& safetyPolicy)
    : recoveryService_(diskReader ? std::make_unique<RecoveryService>(diskReader, safetyPolicy) : nullptr) {
    recoverysuite::logging::Logger::instance().info("GUIRecoveryService initialized");
}

GUIRecoveryService::~GUIRecoveryService() {
    // Clean up any ongoing operations
    operationCancellationTokens_.clear();
    recoverysuite::logging::Logger::instance().info("GUIRecoveryService destroyed");
}

std::vector<models::StorageDevice> GUIRecoveryService::enumerateStorageDevices() {
    // Note: This is a simplified implementation.
    // In a real implementation, we would use the DiskManager or similar
    // to enumerate actual storage devices.

    if (!recoveryService_) {
        recoverysuite::logging::Logger::instance().info("No recovery service available, returning empty device list");
        // For now, return an empty vector as we don't have access to the disk manager here
        // A full implementation would delegate to the underlying recovery service or disk manager
        return std::vector<models::StorageDevice>();
    }

    recoverysuite::logging::Logger::instance().info("Enumerating storage devices (GUI level)");

    // For now, return an empty vector as we don't have access to the disk manager here
    // A full implementation would delegate to the underlying recovery service or disk manager
    return std::vector<models::StorageDevice>();
}

models::StorageDevice GUIRecoveryService::getStorageDeviceInfo(const std::string& devicePath) {
    models::StorageDevice device;
    device.devicePath = devicePath;

    if (!recoveryService_) {
        recoverysuite::logging::Logger::instance().info("No recovery service available, returning default device info");
        return device;
    }

    try {
        // Check if we have a valid disk reader first
        auto diskReader = recoveryService_->getDiskReader();
        if (!diskReader) {
            // Return default device info when no disk reader is available
            recoverysuite::logging::Logger::instance().info("No disk reader available, returning default device info");
            return device;
        }

        // Get disk info from the underlying recovery service
        auto diskInfo = recoveryService_->getDiskInfo();

        device.modelNumber = diskInfo.modelNumber;
        device.serialNumber = diskInfo.serialNumber;
        device.totalBytes = static_cast<uint64_t>(diskInfo.totalSectors) * diskInfo.bytesPerSector;
        device.bytesPerSector = diskInfo.bytesPerSector;
        device.isReadOnly = diskInfo.isReadOnly;

    } catch (const std::exception& e) {
        recoverysuite::logging::Logger::instance().error("Error getting storage device info: " + std::string(e.what()));
        // Return default device with error info in devicePath or similar
    }

    return device;
}

std::vector<models::Partition> GUIRecoveryService::getDevicePartitions(const std::string& devicePath) {
    // Note: This is a simplified implementation.
    // A full implementation would use partition enumeration capabilities
    // from the disk layer.

    recoverysuite::logging::Logger::instance().info("Getting partitions for device: " + devicePath);

    // For now, return an empty vector as we don't have direct access to partition enumeration
    // A full implementation would delegate to the underlying disk management layer
    return std::vector<models::Partition>();
}

models::AnalysisResult GUIRecoveryService::analyzeFilesystem(const std::string& devicePath, uint64_t startSector, uint64_t numSectors) {
    models::AnalysisResult result;
    result.success = false;
    result.errorMessage = "Recovery service not available";

    if (!recoveryService_) {
        recoverysuite::logging::Logger::instance().info("No recovery service available for filesystem analysis");
        return result;
    }

    try {
        recoverysuite::logging::Logger::instance().info("GUI requesting filesystem analysis: " + devicePath +
                                                       ", start_sector=" + std::to_string(startSector) +
                                                       ", num_sectors=" + std::to_string(numSectors));

        // Call the underlying recovery service
        auto serviceResult = recoveryService_->analyzeFilesystem(startSector, numSectors);

        // Convert to GUI model
        result.success = serviceResult.success;
        result.errorMessage = serviceResult.errorMessage;

        // Populate basic filesystem info from analysis results
        if (serviceResult.success && !serviceResult.analysisResults.empty()) {
            result.filesystemInfo.type = serviceResult.analysisResults["filesystem_type"];
            result.filesystemInfo.version = serviceResult.analysisResults["version"];
            result.filesystemInfo.totalSizeBytes = std::stoull(serviceResult.analysisResults["total_size_bytes"]);
            result.filesystemInfo.usedSizeBytes = std::stoull(serviceResult.analysisResults["used_size_bytes"]);
            result.filesystemInfo.freeSizeBytes = std::stoull(serviceResult.analysisResults["free_size_bytes"]);
            result.filesystemInfo.clusterSizeBytes = std::stoul(serviceResult.analysisResults["cluster_size_bytes"]);
            result.filesystemInfo.volumeLabel = serviceResult.analysisResults["volume_label"];
            result.filesystemInfo.serialNumber = serviceResult.analysisResults["serial_number"];
            result.filesystemInfo.isReadOnly = (serviceResult.analysisResults["is_read_only"] == "true");
            result.filesystemInfo.isCorrupted = (serviceResult.analysisResults["is_corrupted"] == "true");
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = "Exception during analysis: " + std::string(e.what());
        recoverysuite::logging::Logger::instance().error("Exception during filesystem analysis: " + std::string(e.what()));
    }

    return result;
}

models::RecoveryResult GUIRecoveryService::recoverFiles(const std::string& devicePath, uint64_t startSector, uint64_t numSectors,
                                                       const std::string& outputPath,
                                                       std::function<void(const models::RecoveryProgress&)> progressCallback,
                                                       std::function<bool()> cancellationToken) {
    models::RecoveryResult result;
    result.success = false;
    result.errorMessage = "Recovery service not available";

    if (!recoveryService_) {
        recoverysuite::logging::Logger::instance().info("No recovery service available for file recovery");
        return result;
    }

    try {
        recoverysuite::logging::Logger::instance().info("GUI requesting file recovery: " + devicePath +
                                                       ", start_sector=" + std::to_string(startSector) +
                                                       ", num_sectors=" + std::to_string(numSectors) +
                                                       ", output_path=" + outputPath);

        // Generate operation ID for tracking
        std::string operationId = "file_recovery_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        // Register cancellation token if provided
        if (cancellationToken) {
            operationCancellationTokens_[operationId] = cancellationToken;
        }

        // TODO: Implement actual progress reporting and cancellation checking
        // For now, we'll call the underlying service and simulate progress

        // Call the underlying recovery service
        auto serviceResult = recoveryService_->recoverFiles(startSector, numSectors);

        // Unregister cancellation token
        operationCancellationTokens_.erase(operationId);

        // Convert to GUI model
        result.success = serviceResult.success;
        result.errorMessage = serviceResult.errorMessage;
        result.operationType = "file_recovery";
        result.processedBytes = static_cast<uint64_t>(numSectors) * 512;  // Assuming 512 byte sectors
        result.recoveredBytes = serviceResult.success ? result.processedBytes : 0;
        result.failedBytes = serviceResult.success ? 0 : result.processedBytes;
        result.recoveredItemsCount = serviceResult.success ? static_cast<uint64_t>(serviceResult.recoveredFiles.size()) : 0;
        result.failedItemsCount = serviceResult.success ? 0 : 1;  // Simplified
        result.startTime = std::chrono::system_clock::now() - std::chrono::seconds(5);  // Placeholder
        result.endTime = std::chrono::system_clock::now();

        // Add details
        result.details["recovered_file_count"] = std::to_string(serviceResult.recoveredFiles.size());

        // If we have a progress callback, simulate some progress updates
        if (progressCallback && serviceResult.success) {
            models::RecoveryProgress progress;
            progress.operationId = operationId;
            progress.operationType = "file_recovery";
            progress.status = "completed";
            progress.percentage = 100;
            progress.currentStep = "Recovery completed";
            progress.processedBytes = result.processedBytes;
            progress.totalBytes = result.processedBytes;
            progress.elapsedSeconds = 5;  // Placeholder
            progress.estimatedRemainingSeconds = 0;
            progress.recoveredItemsCount = result.recoveredItemsCount;
            progress.failedItemsCount = result.failedItemsCount;
            progress.isCancellable = false;
            progress.isPausable = false;
            progress.timestamp = std::chrono::system_clock::now();

            progressCallback(progress);
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = "Exception during file recovery: " + std::string(e.what());
        recoverysuite::logging::Logger::instance().error("Exception during file recovery: " + std::string(e.what()));

        // Clean up cancellation token if registered
        // Note: In a real implementation, we'd need to track the operationId properly
    }

    return result;
}

models::RecoveryResult GUIRecoveryService::recoverMetadata(const std::string& devicePath, uint64_t startSector, uint64_t numSectors,
                                                          const std::string& outputPath,
                                                          std::function<void(const models::RecoveryProgress&)> progressCallback,
                                                          std::function<bool()> cancellationToken) {
    models::RecoveryResult result;
    result.success = false;
    result.errorMessage = "Recovery service not available";

    if (!recoveryService_) {
        recoverysuite::logging::Logger::instance().info("No recovery service available for metadata recovery");
        return result;
    }

    try {
        recoverysuite::logging::Logger::instance().info("GUI requesting metadata recovery: " + devicePath +
                                                       ", start_sector=" + std::to_string(startSector) +
                                                       ", num_sectors=" + std::to_string(numSectors) +
                                                       ", output_path=" + outputPath);

        // Generate operation ID for tracking
        std::string operationId = "metadata_recovery_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        // Register cancellation token if provided
        if (cancellationToken) {
            operationCancellationTokens_[operationId] = cancellationToken;
        }

        // Call the underlying recovery service
        auto serviceResult = recoveryService_->recoverMetadata(startSector, numSectors);

        // Unregister cancellation token
        operationCancellationTokens_.erase(operationId);

        // Convert to GUI model
        result.success = serviceResult.success;
        result.errorMessage = serviceResult.errorMessage;
        result.operationType = "metadata_recovery";
        result.processedBytes = static_cast<uint64_t>(numSectors) * 512;  // Assuming 512 byte sectors
        result.recoveredBytes = serviceResult.success ? result.processedBytes : 0;
        result.failedBytes = serviceResult.success ? 0 : result.processedBytes;
        result.recoveredItemsCount = serviceResult.success ? static_cast<uint64_t>(serviceResult.recoveredMetadata.size()) : 0;
        result.failedItemsCount = serviceResult.success ? 0 : 1;  // Simplified
        result.startTime = std::chrono::system_clock::now() - std::chrono::seconds(5);  // Placeholder
        result.endTime = std::chrono::system_clock::now();

        // Add details
        result.details["recovered_metadata_count"] = std::to_string(serviceResult.recoveredMetadata.size());

        // If we have a progress callback, simulate some progress updates
        if (progressCallback && serviceResult.success) {
            models::RecoveryProgress progress;
            progress.operationId = operationId;
            progress.operationType = "metadata_recovery";
            progress.status = "completed";
            progress.percentage = 100;
            progress.currentStep = "Metadata recovery completed";
            progress.processedBytes = result.processedBytes;
            progress.totalBytes = result.processedBytes;
            progress.elapsedSeconds = 5;  // Placeholder
            progress.estimatedRemainingSeconds = 0;
            progress.recoveredItemsCount = result.recoveredItemsCount;
            progress.failedItemsCount = result.failedItemsCount;
            progress.isCancellable = false;
            progress.isPausable = false;
            progress.timestamp = std::chrono::system_clock::now();

            progressCallback(progress);
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = "Exception during metadata recovery: " + std::string(e.what());
        recoverysuite::logging::Logger::instance().error("Exception during metadata recovery: " + std::string(e.what()));

        // Clean up cancellation token if registered
        // Note: In a real implementation, we'd need to track the operationId properly
    }

    return result;
}

models::RecoveryResult GUIRecoveryService::carveFiles(const std::string& devicePath, uint64_t startSector, uint64_t numSectors,
                                                     const std::string& outputPath,
                                                     std::function<void(const models::RecoveryProgress&)> progressCallback,
                                                     std::function<bool()> cancellationToken) {
    models::RecoveryResult result;
    result.success = false;
    result.errorMessage = "Recovery service not available";

    if (!recoveryService_) {
        recoverysuite::logging::Logger::instance().info("No recovery service available for carving");
        return result;
    }

    try {
        recoverysuite::logging::Logger::instance().info("GUI requesting carving: " + devicePath +
                                                       ", start_sector=" + std::to_string(startSector) +
                                                       ", num_sectors=" + std::to_string(numSectors) +
                                                       ", output_path=" + outputPath);

        // Generate operation ID for tracking
        std::string operationId = "carving_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        // Register cancellation token if provided
        if (cancellationToken) {
            operationCancellationTokens_[operationId] = cancellationToken;
        }

        // Call the underlying recovery service
        auto serviceResult = recoveryService_->carveFiles(startSector, numSectors);

        // Unregister cancellation token
        operationCancellationTokens_.erase(operationId);

        // Convert to GUI model
        result.success = serviceResult.success;
        result.errorMessage = serviceResult.errorMessage;
        result.operationType = "carving";
        result.processedBytes = static_cast<uint64_t>(numSectors) * 512;  // Assuming 512 byte sectors
        result.recoveredBytes = serviceResult.success ? result.processedBytes : 0;
        result.failedBytes = serviceResult.success ? 0 : result.processedBytes;
        result.recoveredItemsCount = serviceResult.success ? static_cast<uint64_t>(serviceResult.carvedFiles.size()) : 0;
        result.failedItemsCount = serviceResult.success ? 0 : 1;  // Simplified
        result.startTime = std::chrono::system_clock::now() - std::chrono::seconds(5);  // Placeholder
        result.endTime = std::chrono::system_clock::now();

        // Add details
        result.details["carved_file_count"] = std::to_string(serviceResult.carvedFiles.size());

        // If we have a progress callback, simulate some progress updates
        if (progressCallback && serviceResult.success) {
            models::RecoveryProgress progress;
            progress.operationId = operationId;
            progress.operationType = "carving";
            progress.status = "completed";
            progress.percentage = 100;
            progress.currentStep = "Carving completed";
            progress.processedBytes = result.processedBytes;
            progress.totalBytes = result.processedBytes;
            progress.elapsedSeconds = 5;  // Placeholder
            progress.estimatedRemainingSeconds = 0;
            progress.recoveredItemsCount = result.recoveredItemsCount;
            progress.failedItemsCount = result.failedItemsCount;
            progress.isCancellable = false;
            progress.isPausable = false;
            progress.timestamp = std::chrono::system_clock::now();

            progressCallback(progress);
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = "Exception during carving: " + std::string(e.what());
        recoverysuite::logging::Logger::instance().error("Exception during carving: " + std::string(e.what()));

        // Clean up cancellation token if registered
        // Note: In a real implementation, we'd need to track the operationId properly
    }

    return result;
}

bool GUIRecoveryService::isSectorRangeValid(const std::string& devicePath, uint64_t startSector, uint64_t numSectors) const {
    if (!recoveryService_) {
        recoverysuite::logging::Logger::instance().info("No recovery service available, assuming sector range is invalid");
        return false;
    }

    try {
        // Get the disk reader for this device and delegate to the underlying service
        auto diskReader = getDiskReaderForDevice(devicePath);
        if (diskReader) {
            // Create a temporary recovery service with this disk reader
            auto tempService = std::make_unique<RecoveryService>(diskReader);
            return tempService->isSectorRangeValid(startSector, numSectors);
        }
    } catch (const std::exception& e) {
        recoverysuite::logging::Logger::instance().error("Error validating sector range: " + std::string(e.what()));
    }

    return false;
}

recoverysuite::disk::DeviceInformation GUIRecoveryService::getDiskInfo(const std::string& devicePath) const {
    if (!recoveryService_) {
        recoverysuite::logging::Logger::instance().info("No recovery service available, returning empty disk info");
        // Return empty device information on error
        recoverysuite::disk::DeviceInformation info;
        return info;
    }

    try {
        // Get the disk reader for this device and delegate to the underlying service
        auto diskReader = getDiskReaderForDevice(devicePath);
        if (diskReader) {
            // Create a temporary recovery service with this disk reader
            auto tempService = std::make_unique<RecoveryService>(diskReader);
            return tempService->getDiskInfo();
        }
    } catch (const std::exception& e) {
        recoverysuite::logging::Logger::instance().error("Error getting disk info: " + std::string(e.what()));
        throw;  // Re-throw to let caller handle
    }

    // Return empty device information on error
    recoverysuite::disk::DeviceInformation info;
    return info;
}

uint64_t GUIRecoveryService::extractDiskNumber(const std::string& /*devicePath*/) const {
    // Simple implementation - in reality this would be platform-specific
    // For Linux: /dev/sda -> 0, /dev/sdb -> 1, etc.
    // For Windows: \\\\.\\PhysicalDrive0 -> 0, etc.

    // This is a placeholder implementation
    return 0;
}

std::shared_ptr<recoverysuite::disk::IDiskReader> GUIRecoveryService::getDiskReaderForDevice(const std::string& /*devicePath*/) const {
    // Delegate to the underlying recovery service's disk reader
    // In a more complex implementation, we might need to create a new reader
    // specific to this device path
    return recoveryService_->getDiskReader();  // This method was added to RecoveryService.h
}

} // namespace service
} // namespace application
} // namespace recoverysuite