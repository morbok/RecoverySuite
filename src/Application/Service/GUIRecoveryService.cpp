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
#include "Recovery/fat_root_files.h"
#include "Recovery/ntfs_mft.h"

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
    if (operation.operationType != "carving" && operation.operationType != "metadata_recovery" && operation.operationType != "file_recovery" && operation.operationType != "file_tree_recovery" && operation.operationType != "ntfs_mft_snapshot" && operation.operationType != "ntfs_resident_recovery" && operation.operationType != "ntfs_tree_recovery" && operation.operationType != "ntfs_checked_recovery")
        return "Choose signature carving, a metadata snapshot or a supported FAT/NTFS file-recovery method";
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
            const bool carving = operation.operationType == "carving";
            const bool tree = operation.operationType == "file_tree_recovery";
            const auto totalBytes = operation.numSectors * imageReader_->getSectorSize();
            result.processedBytesKnown = true; result.failedBytesKnown = true; result.failedItemsKnown = true;
            const auto report = [&](uint64_t processed) {
                result.processedBytes = processed;
                if (!progressCallback) return;
                models::RecoveryProgress progress{};
                progress.operationType = operation.operationType; progress.status = "running";
                progress.currentStep = carving ? "Scanning image and saving signature candidates" : "Reading and saving filesystem data";
                progress.processedBytes = processed; progress.totalBytes = carving ? totalBytes : 0;
                progress.percentage = carving ? static_cast<uint32_t>((static_cast<long double>(processed) / totalBytes) * 100) : 0;
                progress.recoveredItemsCount = result.recoveredItemsCount; progress.failedItemsCount = result.failedItemsCount;
                progress.isCancellable = true; progress.timestamp = std::chrono::system_clock::now();
                progress.elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(progress.timestamp - result.startTime).count();
                progressCallback(progress);
            };
            report(0);
            const auto saveItem = [&](const std::string& name, const std::vector<uint8_t>& data) {
                    if (result.recoveredItemsCount + result.failedItemsCount >= 10000)
                        throw std::runtime_error("Maximum output item count reached");
                    try {
                        if(tree||operation.operationType=="ntfs_tree_recovery")exporter.writeTree(name,data,operation.verifyAfterRecovery,cancellationToken);
                        else exporter.write(name, data, operation.verifyAfterRecovery, cancellationToken);
                        result.recoveredItems.push_back({name, static_cast<uint64_t>(data.size())});
                        ++result.recoveredItemsCount; result.recoveredBytes += data.size(); result.outputWritten = true;
                    } catch (const std::exception& failure) {
                        if (!cancelled()) {
                            ++result.failedItemsCount; result.failedBytes += data.size();
                            result.details[name] = failure.what();
                        }
                    }
                };
            recovery::CarvingSummary summary{};
            if(operation.operationType=="ntfs_checked_recovery") {
                const auto files=recovery::recoverNtfsChecked(*imageReader_,operation.startSector,operation.numSectors,{},[&](const auto& file) {
                    try {
                        exporter.writeStream(file.path,file.size,file.read,operation.verifyAfterRecovery,true,cancellationToken);
                        result.recoveredItems.push_back({file.path,file.size});++result.recoveredItemsCount;result.recoveredBytes+=file.size;result.outputWritten=true;
                        result.details[file.path]=file.confidence;
                    }catch(const std::exception& failure){if(!cancelled()){++result.failedItemsCount;result.failedBytes+=file.size;result.details[file.path]=failure.what();}}
                },report,cancellationToken);
                summary.processedBytes=files.processedBytes;summary.cancelled=cancelled();summary.skippedCandidates=files.invalidFiles;
                result.details["Invalid NTFS files"]=std::to_string(files.invalidFiles);
                result.details["NTFS consistency checks"]="MFT and cluster allocation bitmaps plus parent directory index membership checked";
            } else if (carving) {
                summary = recovery::carveSignatures(*imageReader_,operation.startSector,operation.numSectors,
                    saveItem,report,cancellationToken);
            } else if(operation.operationType=="ntfs_resident_recovery"||operation.operationType=="ntfs_tree_recovery") {
                const auto files=(operation.operationType=="ntfs_tree_recovery"?recovery::recoverNtfsTreeFiles:recovery::recoverNtfsRootFiles)(*imageReader_,operation.startSector,operation.numSectors,saveItem,report,cancellationToken);
                summary.processedBytes=files.processedBytes;summary.cancelled=cancelled();
                summary.skippedCandidates=files.invalidFiles+files.nonresidentFiles+files.unsupportedFiles;
                result.details["Nonresident NTFS root files reconstructed"]=std::to_string(files.recoveredNonresidentFiles);
                result.details["Invalid NTFS root files"]=std::to_string(files.invalidFiles);
                result.details["Nonresident NTFS root files not reconstructed"]=std::to_string(files.nonresidentFiles);
                result.details["Unsupported NTFS root-file features"]=std::to_string(files.unsupportedFiles);
                result.details["Unused user-record slots skipped"]=std::to_string(files.unusedRecords);
                result.details["Directory records skipped"]=std::to_string(files.directoryRecords);
                result.details["Extension records skipped"]=std::to_string(files.extensionRecords);
                result.details["Non-root files skipped"]=std::to_string(files.nonRootFiles);
                result.details["Other root names not exported"]=std::to_string(files.otherRootNames);
            } else if(operation.operationType=="ntfs_mft_snapshot") {
                const auto mft=recovery::readNtfsMft(*imageReader_,operation.startSector,operation.numSectors,report,cancellationToken);
                result.details["MFT record bytes"]=std::to_string(mft.recordSize);
                result.details["MFT in-use record flags"]=std::to_string(mft.inUseRecords);
                result.details["MFT unused record flags"]=std::to_string(mft.unusedRecords);
                result.details["MFT zero-filled record slots"]=std::to_string(mft.blankRecords);
                saveItem("NTFS_BootSector",mft.bootSector);
                if(!cancelled())saveItem("NTFS_MFTInitialized",mft.initializedRecords);
                summary.processedBytes=mft.processedBytes;summary.cancelled=cancelled();
            } else if (operation.operationType == "file_recovery" || tree) {
                const auto files=recovery::recoverFatRootFiles(*imageReader_,operation.startSector,operation.numSectors,saveItem,report,cancellationToken,tree);
                summary.processedBytes=files.processedBytes;summary.cancelled=files.cancelled;summary.skippedCandidates=files.invalidFiles+files.invalidDirectories;
                result.details[tree?"Invalid or unsupported files":"Invalid or unsupported root files"]=std::to_string(files.invalidFiles);
                result.details["Long names rejected (8.3 fallback)"]=std::to_string(files.longNameFallbacks);
                result.details["Deleted entries not reconstructed"]=std::to_string(files.deletedEntries);
                if(tree) {
                    result.details["Directories traversed"]=std::to_string(files.traversedDirectories);
                    result.details["Invalid or unsupported directories"]=std::to_string(files.invalidDirectories);
                }else result.details["Subdirectories not traversed"]=std::to_string(files.directories);
            } else {
                recovery::RecoveryCapabilityRegistry registry;
                registry.registerCapability(recovery::RecoveryCapability::METADATA_RECOVERY);
                recovery::RecoverySafetyPolicy policy;
                recovery::MetadataRecovery metadata(registry,policy,imageReader_.get(),cancellationToken);
                std::map<std::string,std::vector<uint8_t>> regions;
                const bool read = metadata.recoverMetadata(true,true,operation.startSector,operation.numSectors,
                    true,true,true,true,regions);
                if (!read && !cancelled()) throw std::runtime_error("Cannot snapshot FAT metadata: unsupported format, invalid geometry, truncated range or region above 16 MiB");
                for (const auto& [name,data] : regions) {
                    if (cancelled()) break;
                    summary.processedBytes += data.size();
                    saveItem(name,data); report(summary.processedBytes);
                }
                summary.cancelled = cancelled();
            }
            result.processedBytes = summary.processedBytes; result.cancelled = summary.cancelled;
            if (carving) result.details["Skipped incomplete or oversized candidates"] = std::to_string(summary.skippedCandidates);
            result.details["Verification"] = operation.verifyAfterRecovery ? "Saved bytes were read back and compared" : "Not requested";
            result.success = !result.cancelled && !result.failedItemsCount && !summary.skippedCandidates;
            if (result.failedItemsCount) result.errorMessage = "Some candidates could not be saved; existing files were kept";
            else if (summary.skippedCandidates) result.errorMessage = carving ? "Some signature candidates were incomplete or larger than 16 MiB" : "Some files or directories have invalid metadata or unsupported recovery features";
        }
    } catch (const std::exception& failure) { result.errorMessage = failure.what(); result.success = false; }
    result.cancelled = result.cancelled || cancelled();
    if (result.cancelled) result.success = false;
    result.endTime = std::chrono::system_clock::now();
    result.details["Export"] = result.outputWritten ? "Only the listed items were saved to the destination" : "No output files were saved";
    result.details["Recovery method"] = operation.operationType == "ntfs_checked_recovery"
        ? "NTFS files with checked allocation and directory index membership, attribute-list extensions and streaming output up to 2 GiB/file. File content may still be damaged."
        : operation.operationType == "ntfs_tree_recovery"
        ? "In-use NTFS files with sequence-checked parent paths, limited to 32 directory levels. No index/bitmap validation or deleted files."
        : operation.operationType == "ntfs_resident_recovery"
        ? "In-use NTFS root files with unnamed resident/nonresident DATA (16 MiB/file) and sequence-matching parent references. One preferred name per record. No index/bitmap cross-check, deleted/recursive recovery, attribute-list extensions, sparse/compressed/encrypted DATA or alternate-stream export."
        : operation.operationType == "ntfs_mft_snapshot"
        ? "NTFS boot and initialized MFT bytes. Mapping pairs, FILE headers and update-sequence fixups checked; raw on-disk bytes preserved. No file reconstruction, mirror fallback or attribute-list extensions."
        : operation.operationType == "metadata_recovery"
        ? "FAT metadata snapshot: boot sector and FAT copies; FAT12/16 root entries included. No repair, FAT32 root chain or NTFS MFT reconstruction."
        : operation.operationType == "file_tree_recovery" ? "Allocated FAT12/16/32 files with bounded recursive directory traversal. Deleted chains are not inferred; empty directories and original permissions/timestamps are not exported."
        : operation.operationType == "file_recovery" ? "Allocated FAT12/16/32 root files using verified long names or 8.3 aliases and cluster chains. No deleted-file guessing or subdirectory traversal."
        : "Signature candidates only; file structure and content may still be damaged";
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
    models::AnalysisResult result{};
    std::unique_lock lock(imageMutex_, std::try_to_lock);
    if (!lock.owns_lock()) {result.errorMessage="A recovery operation is already active";return result;}
    try {
        const auto reader=getDiskReaderForDevice(devicePath);
        if (!reader) throw std::runtime_error("Recovery service not available");
        if (imageReader_) imageReader_->validateIdentity();
        const auto backend=recoveryService_->analyzeFilesystem(startSector,numSectors);
        result.success=backend.success;result.errorMessage=backend.errorMessage;result.properties=backend.analysisResults;
        if (result.success) {
            const auto number=[&](const std::string& key) -> uint64_t {
                const auto it=result.properties.find(key);
                if (it==result.properties.end() || it->second=="unknown") return 0;
                return std::stoull(it->second);
            };
            auto& info=result.filesystemInfo;
            info.type=result.properties["Filesystem Type"];info.version=result.properties["Version"];
            info.volumeLabel=result.properties["Volume Label"];info.serialNumber=result.properties["Volume Serial Number"];
            info.totalSizeBytes=number("Total Size");info.clusterSizeBytes=number("Bytes Per Sector")*number("Sectors Per Cluster");
            info.isReadOnly=reader->getDiskInfo().isReadOnly;
        }
    } catch(const std::exception& failure) {result.success=false;result.errorMessage=failure.what();}
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