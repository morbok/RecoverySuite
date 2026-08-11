#ifndef RECOVERYSUITE_RECOVERY_OUTPUTEXPORTER_H
#define RECOVERYSUITE_RECOVERY_OUTPUTEXPORTER_H

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
 * OutputExporter
 * Exports recovered data to output storage
 */
class OutputExporter {
public:
    // Constructor
    OutputExporter(
        const RecoveryCapabilityRegistry& capabilityRegistry,
        const RecoverySafetyPolicy& safetyPolicy = RecoverySafetyPolicy(),
        recoverysuite::disk::IDiskReader* diskReader = nullptr
    );

    // Destructor
    ~OutputExporter() = default;

    // Export recovered data to the specified output location
    // Returns true if export was successful
    bool exportData(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        uint64_t startSector,
        uint64_t numSectors,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid,
        const std::map<std::string, std::vector<uint8_t>>& dataToExport
    );

    // Export a specific data item
    bool exportDataItem(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        uint64_t startSector,
        uint64_t numSectors,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid,
        const std::string& dataName,
        const std::vector<uint8_t>& data,
        uint64_t outputStartSector
    );

private:
    // The capability registry to check against
    const RecoveryCapabilityRegistry& capabilityRegistry_;

    // The safety policy to use for validation
    const RecoverySafetyPolicy& safetyPolicy_;

    // The disk reader to use for accessing disk data (for reading source if needed)
    recoverysuite::disk::IDiskReader* diskReader_;

    // The operation validator
    RecoveryOperationValidator validator_;

    // Helper to check if a sector range is valid for the disk
    bool isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const;

    // Helper to write sectors to disk (simulated)
    bool writeSectors(
        uint64_t startSector,
        uint64_t numSectors,
        const std::vector<uint8_t>& buffer
    ) const;

    // Helper to calculate required sectors for data size
    uint64_t calculateRequiredSectors(uint64_t dataSizeBytes) const;
};

} // namespace recovery
} // namespace recoverysuite

#endif // RECOVERYSUITE_RECOVERY_OUTPUTEXPORTER_H