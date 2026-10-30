#include "GUIRecoveryService.h"
#include "RecoveryService.h"
#include "../../Logging/Logger.h"
#include <stdexcept>
#include <chrono>
#include <thread>
#include <filesystem>
#include <limits>
#include "safe_file_exporter.h"
#include "Recovery/signature_carver.h"

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
    if (!imageReader_) return {};
    return {getStorageDeviceInfo(imageReader_->sourcePath())};
}

models::StorageDevice GUIRecoveryService::openImage(const std::string& path, uint32_t sectorSize) {
    std::unique_lock lock(imageMutex_, std::try_to_lock);
    if (!lock.owns_lock()) throw std::runtime_error("Wait for recovery to finish before changing the source");
    auto reader = std::make_shared<disk::DiskImageReader>(sectorSize);
    reader->open(path);
    auto service = std::make_unique<RecoveryService>(reader);
    imageReader_ = std::move(reader);
    recoveryService_ = std::move(service);
    return getStorageDeviceInfo(imageReader_->sourcePath());
}

std::string GUIRecoveryService::validateImageOperation(const models::RecoveryOperation& operation) const {
    if (!imageReader_ || !imageReader_->isOpen()) return "Open a disk image first";
    try { imageReader_->validateIdentity(); }
    catch (const std::exception& failure) { return failure.what(); }
    if (operation.operationType != "carving") return "Only signature carving is available in the image workflow";
    std::error_code error;
    const auto source = std::filesystem::canonical(operation.targetDevicePath, error);
    if (error || source.string() != imageReader_->sourcePath()) return "The operation source does not match the opened image";
    if (!operation.numSectors || operation.startSector >= imageReader_->getTotalSectors() ||
        operation.numSectors > imageReader_->getTotalSectors() - operation.startSector ||
        operation.numSectors > std::numeric_limits<uint64_t>::max() / imageReader_->getSectorSize())
        return "The sector range is outside the opened image";
    if (operation.createLogFile) return "Separate log file output is not available";
    try { SafeFileExporter destination(operation.outputPath); }
    catch (const std::exception& failure) { return failure.what(); }
    return {};
}
std::string GUIRecoveryService::validateOperation(const models::RecoveryOperation& operation) const {
    std::unique_lock lock(imageMutex_, std::try_to_lock);
    if (!lock.owns_lock()) return "A recovery operation is already active";
    return validateImageOperation(operation);
}
models::RecoveryResult GUIRecoveryService::executeOperation(const models::RecoveryOperation& operation,
    std::function<void(const models::RecoveryProgress&)> progressCallback,
    std::function<bool()> cancellationToken) {
    models::RecoveryResult result{};
    result.operationType = operation.operationType; result.destination = operation.outputPath;
    result.startTime = std::chrono::system_clock::now();
    const auto cancelled = [&] { return cancellationToken && cancellationToken(); };
    std::unique_lock lock(imageMutex_, std::try_to_lock);
    try {
        if (!lock.owns_lock()) throw std::runtime_error("A recovery operation is already active");
        if (cancelled()) result.cancelled = true;
        else {
            const auto error = validateImageOperation(operation);
            if (!error.empty()) throw std::runtime_error(error);
            SafeFileExporter exporter(operation.outputPath);
            const auto totalBytes = operation.numSectors * imageReader_->getSectorSize();
            result.processedBytesKnown = true; result.failedBytesKnown = true; result.failedItemsKnown = true;
            const auto report = [&](uint64_t processed) {
                result.processedBytes = processed;
                if (!progressCallback) return;
                models::RecoveryProgress progress{};
                progress.operationType = operation.operationType; progress.status = "running";
                progress.currentStep = "Scanning image and saving signature candidates";
                progress.processedBytes = processed; progress.totalBytes = totalBytes;
                progress.percentage = static_cast<uint32_t>((static_cast<long double>(processed) / totalBytes) * 100);
                progress.recoveredItemsCount = result.recoveredItemsCount; progress.failedItemsCount = result.failedItemsCount;
                progress.isCancellable = true; progress.timestamp = std::chrono::system_clock::now();
                progress.elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(progress.timestamp - result.startTime).count();
                progressCallback(progress);
            };
            report(0);
            const auto summary = recovery::carveSignatures(*imageReader_, operation.startSector, operation.numSectors,
                [&](const std::string& name, const std::vector<uint8_t>& data) {
                    if (result.recoveredItemsCount + result.failedItemsCount >= 10000)
                        throw std::runtime_error("Maximum output item count reached");
                    try {
                        exporter.write(name, data, operation.verifyAfterRecovery, cancellationToken);
                        result.recoveredItems.push_back({name, static_cast<uint64_t>(data.size())});
                        ++result.recoveredItemsCount; result.recoveredBytes += data.size(); result.outputWritten = true;
                    } catch (const std::exception& failure) {
                        if (!cancelled()) {
                            ++result.failedItemsCount; result.failedBytes += data.size();
                            result.details[name] = failure.what();
                        }
                    }
                }, report, cancellationToken);
            result.processedBytes = summary.processedBytes; result.cancelled = summary.cancelled;
            result.details["Skipped incomplete or oversized candidates"] = std::to_string(summary.skippedCandidates);
            result.details["Verification"] = operation.verifyAfterRecovery ? "Saved bytes were read back and compared" : "Not requested";
            result.success = !result.cancelled && !result.failedItemsCount && !summary.skippedCandidates;
            if (result.failedItemsCount) result.errorMessage = "Some candidates could not be saved; existing files were kept";
            else if (summary.skippedCandidates) result.errorMessage = "Some signature candidates were incomplete or larger than 16 MiB";
        }
    } catch (const std::exception& failure) { result.errorMessage = failure.what(); result.success = false; }
    result.cancelled = result.cancelled || cancelled();
    if (result.cancelled) result.success = false;
    result.endTime = std::chrono::system_clock::now();
    result.details["Export"] = result.outputWritten ? "Only the listed items were saved to the destination" : "No output files were saved";
    result.details["Recovery method"] = "Signature candidates only; file structure and content may still be damaged";
    if (progressCallback) {
        models::RecoveryProgress progress{};
        progress.operationType = operation.operationType;
        progress.status = result.cancelled ? "cancelled" : (result.success ? "completed" : "failed");
        progress.currentStep = result.cancelled ? "Cancelled; completed output files were kept" : (result.success ? "Scan complete" : result.errorMessage);
        progress.processedBytes = result.processedBytes;
        progress.recoveredItemsCount = result.recoveredItemsCount; progress.failedItemsCount = result.failedItemsCount;
        progress.timestamp = result.endTime;
        progressCallback(progress);
    }
    return result;
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

std::shared_ptr<recoverysuite::disk::IDiskReader> GUIRecoveryService::getDiskReaderForDevice(const std::string& devicePath) const {
    if (imageReader_) {
        std::error_code error;
        if (std::filesystem::canonical(devicePath, error).string() != imageReader_->sourcePath() || error)
            throw std::invalid_argument("Source does not match the opened image");
    }
    if (!recoveryService_) return nullptr;
    // Delegate to the underlying recovery service's disk reader
    // In a more complex implementation, we might need to create a new reader
    // specific to this device path
    return recoveryService_->getDiskReader();  // This method was added to RecoveryService.h
}

} // namespace service
} // namespace application
} // namespace recoverysuite