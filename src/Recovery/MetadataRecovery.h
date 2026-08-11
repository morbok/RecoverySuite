#ifndef RECOVERYSUITE_RECOVERY_METADATARECOVERY_H
#define RECOVERYSUITE_RECOVERY_METADATARECOVERY_H

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
 * MetadataRecovery
 * Recovers filesystem metadata from storage media
 */
class MetadataRecovery {
public:
    // Constructor
    MetadataRecovery(
        const RecoveryCapabilityRegistry& capabilityRegistry,
        const RecoverySafetyPolicy& safetyPolicy = RecoverySafetyPolicy(),
        recoverysuite::disk::IDiskReader* diskReader = nullptr
    );

    // Destructor
    ~MetadataRecovery() = default;

    // Recover filesystem metadata from the specified disk
    // Returns true if recovery was successful
    bool recoverMetadata(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        uint64_t startSector,
        uint64_t numSectors,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid,
        std::map<std::string, std::vector<uint8_t>>& recoveredMetadata
    );

    // Recover specific metadata type (e.g., FAT, MFT, directory entries)
    bool recoverMetadataType(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        uint64_t startSector,
        uint64_t numSectors,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid,
        const std::string& metadataType,
        uint64_t offset,
        uint64_t size,
        std::vector<uint8_t>& recoveredData
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

    // Helper to read sectors from disk
    bool readSectors(
        uint64_t startSector,
        uint64_t numSectors,
        std::vector<uint8_t>& buffer
    ) const;

    // Helper to detect filesystem type from boot sector data
    std::string detectFilesystemType(const std::vector<uint8_t>& bootSectorData) const;

    // Helper to recover FAT tables
    bool recoverFatTables(
        const std::vector<uint8_t>& bootSectorData,
        std::map<std::string, std::vector<uint8_t>>& recoveredMetadata
    ) const;

    // Helper to recover NTFS MFT
    bool recoverNtfsMft(
        const std::vector<uint8_t>& bootSectorData,
        std::map<std::string, std::vector<uint8_t>>& recoveredMetadata
    ) const;
};

} // namespace recovery
} // namespace recoverysuite

#endif // RECOVERYSUITE_RECOVERY_METADATARECOVERY_H