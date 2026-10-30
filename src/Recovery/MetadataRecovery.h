#ifndef RECOVERYSUITE_RECOVERY_METADATARECOVERY_H
#define RECOVERYSUITE_RECOVERY_METADATARECOVERY_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <functional>
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
        recoverysuite::disk::IDiskReader* diskReader = nullptr,
        std::function<bool()> cancelled = {}
    );

    // Destructor
    ~MetadataRecovery() = default;

    // Snapshot actual FAT boot/FAT/root bytes (16 MiB per region); NTFS MFT is unavailable.
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

    /** Check a range without reading or modifying the source. */
    bool isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const;

private:
    // The capability registry to check against
    const RecoveryCapabilityRegistry& capabilityRegistry_;

    // The safety policy to use for validation
    RecoverySafetyPolicy safetyPolicy_;

    // The disk reader to use for accessing disk data
    recoverysuite::disk::IDiskReader* diskReader_;

    std::function<bool()> cancelled_;

    // The operation validator
    RecoveryOperationValidator validator_;




};

} // namespace recovery
} // namespace recoverysuite

#endif // RECOVERYSUITE_RECOVERY_METADATARECOVERY_H