#ifndef RECOVERYSUITE_RECOVERY_CARVINGENGINE_H
#define RECOVERYSUITE_RECOVERY_CARVINGENGINE_H

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
 * CarvingEngine
 * Recovers files from storage media using signature-based carving (file carving)
 */
class CarvingEngine {
public:
    // Constructor
    CarvingEngine(
        const RecoveryCapabilityRegistry& capabilityRegistry,
        const RecoverySafetyPolicy& safetyPolicy = RecoverySafetyPolicy(),
        recoverysuite::disk::IDiskReader* diskReader = nullptr
    );

    // Destructor
    ~CarvingEngine() = default;

    // Perform file carving on the specified disk
    // Returns true if carving was successful
    bool carveFiles(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        uint64_t startSector,
        uint64_t numSectors,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid,
        std::map<std::string, std::vector<uint8_t>>& carvedFiles
    );

    // Carve JPEG/PDF/ZIP candidates with absolute byte offsets. Buffers are limited to 64 MiB total.
    bool carveFileType(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        uint64_t startSector,
        uint64_t numSectors,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid,
        const std::string& fileType,
        std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& carvedFiles
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


};

} // namespace recovery
} // namespace recoverysuite

#endif // RECOVERYSUITE_RECOVERY_CARVINGENGINE_H