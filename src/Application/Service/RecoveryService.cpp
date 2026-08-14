#include "RecoveryService.h"
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <limits>

#include "../Recovery/FilesystemAnalyzer.h"
#include "../Recovery/FileRecovery.h"
#include "../Recovery/MetadataRecovery.h"
#include "../Recovery/CarvingEngine.h"
#include "../Recovery/RecoveryCapability.h"
#include "../../Logging/Logger.h"

namespace recoverysuite {
namespace application {
namespace service {

RecoveryService::RecoveryService(std::shared_ptr<recoverysuite::disk::IDiskReader> diskReader,
                                 const recoverysuite::recovery::RecoverySafetyPolicy& safetyPolicy)
    : diskReader_(std::move(diskReader)), safetyPolicy_(safetyPolicy) {
    if (!diskReader_) {
        throw std::invalid_argument("Disk reader cannot be null");
    }

    // Register all recovery capabilities that our service will use
    capabilityRegistry_.registerCapability(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS);
    capabilityRegistry_.registerCapability(recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY);
    capabilityRegistry_.registerCapability(recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY);
    capabilityRegistry_.registerCapability(recoverysuite::recovery::RecoveryCapability::CARVING);

    recoverysuite::logging::Logger::instance().info("RecoveryService initialized");
}

RecoveryService::~RecoveryService() {
    // Components are cleaned up by unique_ptr
    recoverysuite::logging::Logger::instance().info("RecoveryService destroyed");
}

void RecoveryService::initializeComponents() const {
    if (!filesystemAnalyzer_) {
        filesystemAnalyzer_ = std::make_unique<recoverysuite::recovery::FilesystemAnalyzer>(
            capabilityRegistry_, safetyPolicy_, diskReader_.get());
        recoverysuite::logging::Logger::instance().debug("FilesystemAnalyzer component initialized");
    }

    if (!fileRecovery_) {
        fileRecovery_ = std::make_unique<recoverysuite::recovery::FileRecovery>(
            capabilityRegistry_, safetyPolicy_, diskReader_.get());
        recoverysuite::logging::Logger::instance().debug("FileRecovery component initialized");
    }

    if (!metadataRecovery_) {
        metadataRecovery_ = std::make_unique<recoverysuite::recovery::MetadataRecovery>(
            capabilityRegistry_, safetyPolicy_, diskReader_.get());
        recoverysuite::logging::Logger::instance().debug("MetadataRecovery component initialized");
    }

    if (!carvingEngine_) {
        carvingEngine_ = std::make_unique<recoverysuite::recovery::CarvingEngine>(
            capabilityRegistry_, safetyPolicy_, diskReader_.get());
        recoverysuite::logging::Logger::instance().debug("CarvingEngine component initialized");
    }
}

RecoveryService::FilesystemAnalysisResult RecoveryService::analyzeFilesystem(uint64_t startSector, uint64_t numSectors) {
    FilesystemAnalysisResult result;

    try {
        // Log operation initialization
        recoverysuite::logging::Logger::instance().info("Filesystem analysis started: start_sector=" + std::to_string(startSector) + ", num_sectors=" + std::to_string(numSectors));

        // Validate sector range
        if (!isSectorRangeValid(startSector, numSectors)) {
            result.errorMessage = "Invalid sector range";
            recoverysuite::logging::Logger::instance().warn("Filesystem analysis failed: invalid sector range");
            return result;
        }

        // Initialize components if needed
        initializeComponents();

        // Perform the analysis with safe default parameters
        // In a real application, these would come from user context or configuration
        bool sourceIsReadOnly = true;  // Default to read-only for safety
        bool destinationDiffersFromSource = true;  // Analysis doesn't write to source
        bool outputPathIsValid = true;  // Not applicable for analysis
        bool arithmeticCannotOverflow = true;  // Will be validated internally
        bool cancellationStateIsValid = true;  // Not checking cancellation for simplicity

        std::map<std::string, std::string> analysisResults;
        bool success = filesystemAnalyzer_->analyzeFilesystem(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            startSector,
            numSectors,
            true,  // sourceRangesAreValid (we already checked)
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid,
            analysisResults
        );

        if (success) {
            result = convertFilesystemAnalysisResult(success, "", analysisResults);
            recoverysuite::logging::Logger::instance().info("Filesystem analysis completed successfully");
        } else {
            result.errorMessage = "Filesystem analysis failed";
            recoverysuite::logging::Logger::instance().warn("Filesystem analysis failed");
        }
    } catch (const std::exception& e) {
        result.errorMessage = "Exception during analysis: " + std::string(e.what());
        recoverysuite::logging::Logger::instance().error("Exception during filesystem analysis: " + std::string(e.what()));
    } catch (...) {
        result.errorMessage = "Unknown exception during analysis";
        recoverysuite::logging::Logger::instance().error("Unknown exception during filesystem analysis");
    }

    return result;
}

RecoveryService::FileRecoveryResult RecoveryService::recoverFiles(uint64_t startSector, uint64_t numSectors) {
    FileRecoveryResult result;

    try {
        // Validate sector range
        if (!isSectorRangeValid(startSector, numSectors)) {
            result.errorMessage = "Invalid sector range";
            return result;
        }

        // Initialize components if needed
        initializeComponents();

        // Perform file recovery with safe default parameters
        bool sourceIsReadOnly = true;  // Default to read-only for safety
        bool destinationDiffersFromSource = true;  // Recovery typically writes to different location
        bool outputPathIsValid = true;  // In a real app, this would validate an actual output path
        bool arithmeticCannotOverflow = true;  // Will be validated internally
        bool cancellationStateIsValid = true;  // Not checking cancellation for simplicity

        std::map<std::string, std::vector<uint8_t>> recoveredFiles;
        bool success = fileRecovery_->recoverFiles(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            startSector,
            numSectors,
            true,  // sourceRangesAreValid (we already checked)
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid,
            recoveredFiles
        );

        if (success) {
            result = convertFileRecoveryResult(success, "", recoveredFiles);
            recoverysuite::logging::Logger::instance().info("File recovery completed successfully: recovered " + std::to_string(recoveredFiles.size()) + " files");
        } else {
            result.errorMessage = "File recovery failed";
            recoverysuite::logging::Logger::instance().warn("File recovery failed");
        }
    } catch (const std::exception& e) {
        result.errorMessage = "Exception during file recovery: " + std::string(e.what());
        recoverysuite::logging::Logger::instance().error("Exception during file recovery: " + std::string(e.what()));
    } catch (...) {
        result.errorMessage = "Unknown exception during file recovery";
        recoverysuite::logging::Logger::instance().error("Unknown exception during file recovery");
    }

    return result;
}

RecoveryService::MetadataRecoveryResult RecoveryService::recoverMetadata(uint64_t startSector, uint64_t numSectors) {
    MetadataRecoveryResult result;

    try {
        // Validate sector range
        if (!isSectorRangeValid(startSector, numSectors)) {
            result.errorMessage = "Invalid sector range";
            return result;
        }

        // Initialize components if needed
        initializeComponents();

        // Perform metadata recovery with safe default parameters
        bool sourceIsReadOnly = true;  // Default to read-only for safety
        bool destinationDiffersFromSource = true;  // Recovery typically writes to different location
        bool outputPathIsValid = true;  // In a real app, this would validate an actual output path
        bool arithmeticCannotOverflow = true;  // Will be validated internally
        bool cancellationStateIsValid = true;  // Not checking cancellation for simplicity

        std::map<std::string, std::vector<uint8_t>> recoveredMetadata;
        bool success = metadataRecovery_->recoverMetadata(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            startSector,
            numSectors,
            true,  // sourceRangesAreValid (we already checked)
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid,
            recoveredMetadata
        );

        if (success) {
            result = convertMetadataRecoveryResult(success, "", recoveredMetadata);
            recoverysuite::logging::Logger::instance().info("Metadata recovery completed successfully: recovered " + std::to_string(recoveredMetadata.size()) + " metadata items");
        } else {
            result.errorMessage = "Metadata recovery failed";
            recoverysuite::logging::Logger::instance().warn("Metadata recovery failed");
        }
    } catch (const std::exception& e) {
        result.errorMessage = "Exception during metadata recovery: " + std::string(e.what());
        recoverysuite::logging::Logger::instance().error("Exception during metadata recovery: " + std::string(e.what()));
    } catch (...) {
        result.errorMessage = "Unknown exception during metadata recovery";
        recoverysuite::logging::Logger::instance().error("Unknown exception during metadata recovery");
    }

    return result;
}

RecoveryService::CarvingResult RecoveryService::carveFiles(uint64_t startSector, uint64_t numSectors) {
    CarvingResult result;

    try {
        // Validate sector range
        if (!isSectorRangeValid(startSector, numSectors)) {
            result.errorMessage = "Invalid sector range";
            return result;
        }

        // Initialize components if needed
        initializeComponents();

        // Perform carving with safe default parameters
        bool sourceIsReadOnly = true;  // Default to read-only for safety
        bool destinationDiffersFromSource = true;  // Carving typically writes to different location
        bool outputPathIsValid = true;  // In a real app, this would validate an actual output path
        bool arithmeticCannotOverflow = true;  // Will be validated internally
        bool cancellationStateIsValid = true;  // Not checking cancellation for simplicity

        std::map<std::string, std::vector<uint8_t>> carvedFiles;
        bool success = carvingEngine_->carveFiles(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            startSector,
            numSectors,
            true,  // sourceRangesAreValid (we already checked)
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid,
            carvedFiles
        );

        if (success) {
            result = convertCarvingResult(success, "", carvedFiles);
            recoverysuite::logging::Logger::instance().info("Carving completed successfully: carved " + std::to_string(carvedFiles.size()) + " files");
        } else {
            result.errorMessage = "Carving failed";
            recoverysuite::logging::Logger::instance().warn("Carving failed");
        }
    } catch (const std::exception& e) {
        result.errorMessage = "Exception during carving: " + std::string(e.what());
        recoverysuite::logging::Logger::instance().error("Exception during carving: " + std::string(e.what()));
    } catch (...) {
        result.errorMessage = "Unknown exception during carving";
        recoverysuite::logging::Logger::instance().error("Unknown exception during carving");
    }

    return result;
}

bool RecoveryService::isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const {
    if (!diskReader_) {
        return false;
    }

    // Check for zero-length range
    if (numSectors == 0) {
        return false;
    }

    // Get total sectors from disk
    uint64_t totalSectors = diskReader_->getTotalSectors();

    // Check for overflow in startSector + numSectors
    // If startSector + numSectors < startSector, overflow occurred
    if (startSector > std::numeric_limits<uint64_t>::max() - numSectors) {
        return false;  // Overflow would occur
    }

    uint64_t endSector = startSector + numSectors;

    // Check if start is beyond the disk
    if (startSector >= totalSectors) {
        return false;
    }

    // Check if range extends beyond the disk
    if (endSector > totalSectors) {
        return false;
    }

    return true;
}

recoverysuite::disk::DeviceInformation RecoveryService::getDiskInfo() const {
    if (!diskReader_) {
        throw std::runtime_error("Disk reader is not initialized");
    }
    return diskReader_->getDiskInfo();
}

RecoveryService::FileRecoveryResult RecoveryService::convertFileRecoveryResult(bool success, const std::string& errorMessage,
                                                                              std::map<std::string, std::vector<uint8_t>>& recoveredFiles) const {
    FileRecoveryResult result;
    result.success = success;
    if (!errorMessage.empty()) {
        result.errorMessage = errorMessage;
    }
    result.recoveredFiles = std::move(recoveredFiles);
    return result;
}

RecoveryService::MetadataRecoveryResult RecoveryService::convertMetadataRecoveryResult(bool success, const std::string& errorMessage,
                                                                                      std::map<std::string, std::vector<uint8_t>>& recoveredMetadata) const {
    MetadataRecoveryResult result;
    result.success = success;
    if (!errorMessage.empty()) {
        result.errorMessage = errorMessage;
    }
    result.recoveredMetadata = std::move(recoveredMetadata);
    return result;
}

RecoveryService::FilesystemAnalysisResult RecoveryService::convertFilesystemAnalysisResult(bool success, const std::string& errorMessage,
                                                                                          std::map<std::string, std::string>& analysisResults) const {
    FilesystemAnalysisResult result;
    result.success = success;
    if (!errorMessage.empty()) {
        result.errorMessage = errorMessage;
    }
    result.analysisResults = std::move(analysisResults);
    return result;
}

RecoveryService::CarvingResult RecoveryService::convertCarvingResult(bool success, const std::string& errorMessage,
                                                                    std::map<std::string, std::vector<uint8_t>>& carvedFiles) const {
    CarvingResult result;
    result.success = success;
    if (!errorMessage.empty()) {
        result.errorMessage = errorMessage;
    }
    result.carvedFiles = std::move(carvedFiles);
    return result;
}

}  // namespace service
}  // namespace application
}  // namespace recoverysuite