#ifndef RECOVERYSUITE_RECOVERY_FILESYSTEMANALYZER_H
#define RECOVERYSUITE_RECOVERY_FILESYSTEMANALYZER_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "RecoveryCapability.h"
#include "RecoveryOperationValidator.h"
#include "../Disk/IDiskReader.hpp"

namespace recoverysuite {
namespace recovery {

/**
 * FilesystemAnalyzer
 * Analyzes filesystems on storage media to gather detailed information
 */
class FilesystemAnalyzer {
public:
    // Constructor
    FilesystemAnalyzer(
        const RecoveryCapabilityRegistry& capabilityRegistry,
        const RecoverySafetyPolicy& safetyPolicy = RecoverySafetyPolicy(),
        recoverysuite::disk::IDiskReader* diskReader = nullptr
    );

    // Destructor
    ~FilesystemAnalyzer() = default;

    // Analyze filesystem on the specified disk
    // Returns true if analysis was successful
    bool analyzeFilesystem(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        uint64_t startSector,
        uint64_t numSectors,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid,
        std::map<std::string, std::string>& analysisResults
    );

    // Get detailed information about a specific filesystem type
    bool getFilesystemInfo(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        uint64_t startSector,
        uint64_t numSectors,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid,
        const std::string& filesystemType,
        std::map<std::string, std::string>& info
    );

private:
    // The capability registry to check against
    const RecoveryCapabilityRegistry& capabilityRegistry_;

    // The safety policy to use for validation
    const RecoverySafetyPolicy& safetyPolicy_;

    // The disk reader to use for accessing disk data
    recoverysuite::disk::IDiskReader* diskReader_;

    // The operation validator
    RecoveryOperationValidator validator_;

    // Helper to check if a sector range is valid for the disk
    bool isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const;

    // Helper to analyze FAT filesystem
    bool analyzeFatFilesystem(
        const std::vector<uint8_t>& bootSectorData,
        std::map<std::string, std::string>& analysisResults
    ) const;

    // Helper to analyze NTFS filesystem
    bool analyzeNtfsFilesystem(
        const std::vector<uint8_t>& bootSectorData,
        std::map<std::string, std::string>& analysisResults
    ) const;

    // Helper to read sectors from disk
    bool readSectors(
        uint64_t startSector,
        uint64_t numSectors,
        std::vector<uint8_t>& buffer
    ) const;

    // Helper to detect filesystem type from boot sector data
    std::string detectFilesystemType(const std::vector<uint8_t>& bootSectorData) const;
};

} // namespace recovery
} // namespace recoverysuite

#endif // RECOVERYSUITE_RECOVERY_FILESYSTEMANALYZER_H