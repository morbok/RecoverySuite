#ifndef RECOVERYSUITE_RECOVERY_FILESYSTEMDETECTOR_H
#define RECOVERYSUITE_RECOVERY_FILESYSTEMDETECTOR_H

#include <cstdint>
#include <string>
#include <vector>
#include "RecoveryCapability.h"
#include "RecoveryOperationValidator.h"
#include "../Disk/IDiskReader.hpp"

namespace recoverysuite {
namespace recovery {

/**
 * FilesystemDetector
 * Detects filesystems on storage media
 */
class FilesystemDetector {
public:
    // Constructor
    FilesystemDetector(
        const RecoveryCapabilityRegistry& capabilityRegistry,
        const RecoverySafetyPolicy& safetyPolicy = RecoverySafetyPolicy(),
        recoverysuite::disk::IDiskReader* diskReader = nullptr
    );

    // Destructor
    ~FilesystemDetector() = default;

    // Detect filesystems on the specified disk
    // Returns true if detection was successful
    bool detectFilesystems(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        uint64_t startSector,
        uint64_t numSectors,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid,
        std::vector<std::string>& detectedFilesystems
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

    // Helper to detect filesystem type from boot sector data
    std::string detectFilesystemType(const std::vector<uint8_t>& bootSectorData) const;
};

} // namespace recovery
} // namespace recoverysuite

#endif // RECOVERYSUITE_RECOVERY_FILESYSTEMDETECTOR_H