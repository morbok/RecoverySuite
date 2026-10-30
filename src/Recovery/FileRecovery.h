#ifndef RECOVERYSUITE_RECOVERY_FILERECOVERY_H
#define RECOVERYSUITE_RECOVERY_FILERECOVERY_H

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
 * FileRecovery
 * Recovers files from storage media
 */
class FileRecovery {
public:
    // Constructor
    FileRecovery(
        const RecoveryCapabilityRegistry& capabilityRegistry,
        const RecoverySafetyPolicy& safetyPolicy = RecoverySafetyPolicy(),
        recoverysuite::disk::IDiskReader* diskReader = nullptr
    );

    // Destructor
    ~FileRecovery() = default;

    // Recover files from the specified disk
    // Returns true if recovery was successful
    bool recoverFiles(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        uint64_t startSector,
        uint64_t numSectors,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid,
        std::map<std::string, std::vector<uint8_t>>& recoveredFiles
    );

    // Recover a specific file by name or identifier
    bool recoverFile(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        uint64_t startSector,
        uint64_t numSectors,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid,
        const std::string& fileIdentifier,
        uint64_t fileStartSector,
        uint64_t fileSizeInBytes,
        std::vector<uint8_t>& recoveredData
    );

private:
    // The capability registry to check against
    const RecoveryCapabilityRegistry& capabilityRegistry_;

    // The safety policy to use for validation
    RecoverySafetyPolicy safetyPolicy_;

    // The disk reader to use for accessing disk data
    recoverysuite::disk::IDiskReader* diskReader_;

    // The operation validator
    RecoveryOperationValidator validator_;

    // Helper to check if a sector range is valid for the disk
    bool isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const;


};

} // namespace recovery
} // namespace recoverysuite

#endif // RECOVERYSUITE_RECOVERY_FILERECOVERY_H