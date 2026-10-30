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
    models::RecoveryResult result{};
    result.operationType = "file_recovery";
    result.destination = outputPath;
    result.startTime = std::chrono::system_clock::now();
    result.details["export"] = "Output files have not been saved.";
    try {
        if (cancellationToken && cancellationToken()) {
            result.cancelled = true;
        } else if (!recoveryService_) {
            result.errorMessage = "Recovery service not available";
        } else if (devicePath.empty() || !isSectorRangeValid(devicePath, startSector, numSectors)) {
            result.errorMessage = "Invalid source or sector range";
        } else {
            models::RecoveryProgress progress{};
            progress.operationType = result.operationType;
            progress.status = "running";
            progress.currentStep = "Processing source; detailed progress is not reported by the backend";
            progress.isCancellable = true;
            if (progressCallback) progressCallback(progress);
            if (cancellationToken && cancellationToken()) {
                result.cancelled = true;
            } else {
                const auto backend = recoveryService_->recoverFiles(startSector, numSectors);
                result.success = backend.success;
                result.errorMessage = backend.errorMessage;
                for (const auto& [name, data] : backend.recoveredFiles) {
                    result.recoveredItems.push_back({name, static_cast<uint64_t>(data.size())});
                    result.recoveredBytes += data.size();
                }
                result.recoveredItemsCount = result.recoveredItems.size();
                result.cancelled = cancellationToken && cancellationToken();
                if (result.cancelled) result.success = false;
            }
        }
    } catch (const std::exception& error) {
        result.success = false;
        result.errorMessage = error.what();
    }
    result.endTime = std::chrono::system_clock::now();
    if (progressCallback) {
        models::RecoveryProgress progress{};
        progress.operationType = result.operationType;
        progress.status = result.cancelled ? "cancelled" : (result.success ? "completed" : "failed");
        progress.currentStep = result.cancelled ? "Cancelled" : (result.success ? "Processing complete; output has not been exported" : result.errorMessage);
        progress.elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(result.endTime - result.startTime).count();
        progress.recoveredItemsCount = result.recoveredItemsCount;
        progress.timestamp = result.endTime;
        progressCallback(progress);
    }
    return result;
}

models::RecoveryResult GUIRecoveryService::recoverMetadata(const std::string& devicePath, uint64_t startSector, uint64_t numSectors,
    const std::string& outputPath,
    std::function<void(const models::RecoveryProgress&)> progressCallback,
    std::function<bool()> cancellationToken) {
    models::RecoveryResult result{};
    result.operationType = "metadata_recovery";
    result.destination = outputPath;
    result.startTime = std::chrono::system_clock::now();
    result.details["export"] = "Output files have not been saved.";
    try {
        if (cancellationToken && cancellationToken()) {
            result.cancelled = true;
        } else if (!recoveryService_) {
            result.errorMessage = "Recovery service not available";
        } else if (devicePath.empty() || !isSectorRangeValid(devicePath, startSector, numSectors)) {
            result.errorMessage = "Invalid source or sector range";
        } else {
            models::RecoveryProgress progress{};
            progress.operationType = result.operationType;
            progress.status = "running";
            progress.currentStep = "Processing source; detailed progress is not reported by the backend";
            progress.isCancellable = true;
            if (progressCallback) progressCallback(progress);
            if (cancellationToken && cancellationToken()) {
                result.cancelled = true;
            } else {
                const auto backend = recoveryService_->recoverMetadata(startSector, numSectors);
                result.success = backend.success;
                result.errorMessage = backend.errorMessage;
                for (const auto& [name, data] : backend.recoveredMetadata) {
                    result.recoveredItems.push_back({name, static_cast<uint64_t>(data.size())});
                    result.recoveredBytes += data.size();
                }
                result.recoveredItemsCount = result.recoveredItems.size();
                result.cancelled = cancellationToken && cancellationToken();
                if (result.cancelled) result.success = false;
            }
        }
    } catch (const std::exception& error) {
        result.success = false;
        result.errorMessage = error.what();
    }
    result.endTime = std::chrono::system_clock::now();
    if (progressCallback) {
        models::RecoveryProgress progress{};
        progress.operationType = result.operationType;
        progress.status = result.cancelled ? "cancelled" : (result.success ? "completed" : "failed");
        progress.currentStep = result.cancelled ? "Cancelled" : (result.success ? "Processing complete; output has not been exported" : result.errorMessage);
        progress.elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(result.endTime - result.startTime).count();
        progress.recoveredItemsCount = result.recoveredItemsCount;
        progress.timestamp = result.endTime;
        progressCallback(progress);
    }
    return result;
}

models::RecoveryResult GUIRecoveryService::carveFiles(const std::string& devicePath, uint64_t startSector, uint64_t numSectors,
    const std::string& outputPath,
    std::function<void(const models::RecoveryProgress&)> progressCallback,
    std::function<bool()> cancellationToken) {
    models::RecoveryResult result{};
    result.operationType = "carving";
    result.destination = outputPath;
    result.startTime = std::chrono::system_clock::now();
    result.details["export"] = "Output files have not been saved.";
    try {
        if (cancellationToken && cancellationToken()) {
            result.cancelled = true;
        } else if (!recoveryService_) {
            result.errorMessage = "Recovery service not available";
        } else if (devicePath.empty() || !isSectorRangeValid(devicePath, startSector, numSectors)) {
            result.errorMessage = "Invalid source or sector range";
        } else {
            models::RecoveryProgress progress{};
            progress.operationType = result.operationType;
            progress.status = "running";
            progress.currentStep = "Processing source; detailed progress is not reported by the backend";
            progress.isCancellable = true;
            if (progressCallback) progressCallback(progress);
            if (cancellationToken && cancellationToken()) {
                result.cancelled = true;
            } else {
                const auto backend = recoveryService_->carveFiles(startSector, numSectors);
                result.success = backend.success;
                result.errorMessage = backend.errorMessage;
                for (const auto& [name, data] : backend.carvedFiles) {
                    result.recoveredItems.push_back({name, static_cast<uint64_t>(data.size())});
                    result.recoveredBytes += data.size();
                }
                result.recoveredItemsCount = result.recoveredItems.size();
                result.cancelled = cancellationToken && cancellationToken();
                if (result.cancelled) result.success = false;
            }
        }
    } catch (const std::exception& error) {
        result.success = false;
        result.errorMessage = error.what();
    }
    result.endTime = std::chrono::system_clock::now();
    if (progressCallback) {
        models::RecoveryProgress progress{};
        progress.operationType = result.operationType;
        progress.status = result.cancelled ? "cancelled" : (result.success ? "completed" : "failed");
        progress.currentStep = result.cancelled ? "Cancelled" : (result.success ? "Processing complete; output has not been exported" : result.errorMessage);
        progress.elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(result.endTime - result.startTime).count();
        progress.recoveredItemsCount = result.recoveredItemsCount;
        progress.timestamp = result.endTime;
        progressCallback(progress);
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