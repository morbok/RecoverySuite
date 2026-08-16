#ifndef RECOVERYSUITE_APPLICATION_SERVICE_RECOVERYSERVICE_H
#define RECOVERYSUITE_APPLICATION_SERVICE_RECOVERYSERVICE_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

#include "../Recovery/RecoveryCapability.h"
#include "../Recovery/RecoverySafetyPolicy.h"
#include "../Recovery/FilesystemAnalyzer.h"
#include "../Recovery/FileRecovery.h"
#include "../Recovery/MetadataRecovery.h"
#include "../Recovery/CarvingEngine.h"
#include "../Disk/IDiskReader.hpp"

namespace recoverysuite {
namespace application {
namespace service {

/**
 * RecoveryService
 *
 * A high-level service layer that provides a clean interface to recovery operations.
 * This service abstracts away the complexity of the lower-level recovery components
 * and provides easy-to-use methods for common recovery tasks.
 */
class RecoveryService {
public:
    /**
     * Result of a file recovery operation
     */
    struct FileRecoveryResult {
        bool success;
        std::string errorMessage;
        std::map<std::string, std::vector<uint8_t>> recoveredFiles; // filename -> file data

        FileRecoveryResult() : success(false) {}
    };

    /**
     * Result of a metadata recovery operation
     */
    struct MetadataRecoveryResult {
        bool success;
        std::string errorMessage;
        std::map<std::string, std::vector<uint8_t>> recoveredMetadata; // metadata type -> data

        MetadataRecoveryResult() : success(false) {}
    };

    /**
     * Result of a filesystem analysis operation
     */
    struct FilesystemAnalysisResult {
        bool success;
        std::string errorMessage;
        std::map<std::string, std::string> analysisResults; // property -> value

        FilesystemAnalysisResult() : success(false) {}
    };

    /**
     * Result of a carving operation
     */
    struct CarvingResult {
        bool success;
        std::string errorMessage;
        std::map<std::string, std::vector<uint8_t>> carvedFiles; // filename -> file data

        CarvingResult() : success(false) {}
    };

    /**
     * Constructor
     * @param diskReader The disk reader to use for accessing storage devices
     * @param safetyPolicy Optional safety policy to apply (defaults to strict policy)
     */
    RecoveryService(std::shared_ptr<recoverysuite::disk::IDiskReader> diskReader,
                    const recoverysuite::recovery::RecoverySafetyPolicy& safetyPolicy = recoverysuite::recovery::RecoverySafetyPolicy());

    /**
     * Destructor
     */
    ~RecoveryService();

    /**
     * Analyze the filesystem on a disk or disk range
     *
     * @param startSector Starting sector to analyze (0-based)
     * @param numSectors Number of sectors to analyze
     * @return FilesystemAnalysisResult containing analysis data or error information
     */
    FilesystemAnalysisResult analyzeFilesystem(uint64_t startSector, uint64_t numSectors);

    /**
     * Recover files from a filesystem
     *
     * @param startSector Starting sector of the filesystem (0-based)
     * @param numSectors Number of sectors in the filesystem
     * @return FileRecoveryResult containing recovered files or error information
     */
    FileRecoveryResult recoverFiles(uint64_t startSector, uint64_t numSectors);

    /**
     * Recover filesystem metadata
     *
     * @param startSector Starting sector of the filesystem (0-based)
     * @param numSectors Number of sectors in the filesystem
     * @return MetadataRecoveryResult containing recovered metadata or error information
     */
    MetadataRecoveryResult recoverMetadata(uint64_t startSector, uint64_t numSectors);

    /**
     * Carve files from raw disk data (file carving)
     *
     * @param startSector Starting sector to scan (0-based)
     * @param numSectors Number of sectors to scan
     * @return CarvingResult containing carved files or error information
     */
    CarvingResult carveFiles(uint64_t startSector, uint64_t numSectors);

    /**
     * Check if a sector range is valid on the disk
     *
     * @param startSector Starting sector to check (0-based)
     * @param numSectors Number of sectors to check
     * @return true if the sector range is valid, false otherwise
     */
    bool isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const;

    /**
     * Get disk information
     *
     * @return DeviceInformation structure containing disk details
     */
    recoverysuite::disk::DeviceInformation getDiskInfo() const;

private:
    // The disk reader to use for accessing disk data
    std::shared_ptr<recoverysuite::disk::IDiskReader> diskReader_;

    // The safety policy to use for validation
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy_;

    // The capability registry (managed internally)
    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry_;

    // Internal recovery components (lazy-initialized)
    mutable std::unique_ptr<recoverysuite::recovery::FilesystemAnalyzer> filesystemAnalyzer_;
    mutable std::unique_ptr<recoverysuite::recovery::FileRecovery> fileRecovery_;
    mutable std::unique_ptr<recoverysuite::recovery::MetadataRecovery> metadataRecovery_;
    mutable std::unique_ptr<recoverysuite::recovery::CarvingEngine> carvingEngine_;

public:
    /**
     * @brief Get the disk reader used by this service
     * @return Shared pointer to the disk reader
     */
    std::shared_ptr<recoverysuite::disk::IDiskReader> getDiskReader() const {
        return diskReader_;
    }

    /**
     * Initialize internal recovery components if needed
     */
    void initializeComponents() const;

    /**
     * Convert internal validation result to service result for file recovery
     */
    FileRecoveryResult convertFileRecoveryResult(bool success, const std::string& errorMessage,
                                               std::map<std::string, std::vector<uint8_t>>& recoveredFiles) const;

    /**
     * Convert internal validation result to service result for metadata recovery
     */
    MetadataRecoveryResult convertMetadataRecoveryResult(bool success, const std::string& errorMessage,
                                                       std::map<std::string, std::vector<uint8_t>>& recoveredMetadata) const;

    /**
     * Convert internal validation result to service result for filesystem analysis
     */
    FilesystemAnalysisResult convertFilesystemAnalysisResult(bool success, const std::string& errorMessage,
                                                         std::map<std::string, std::string>& analysisResults) const;

    /**
     * Convert internal validation result to service result for carving
     */
    CarvingResult convertCarvingResult(bool success, const std::string& errorMessage,
                                     std::map<std::string, std::vector<uint8_t>>& carvedFiles) const;
};

} // namespace service
} // namespace application
} // namespace recoverysuite

#endif // RECOVERYSUITE_APPLICATION_SERVICE_RECOVERYSERVICE_H