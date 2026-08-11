#include "OutputExporter.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <sstream>

namespace recoverysuite {
namespace recovery {

OutputExporter::OutputExporter(
    const RecoveryCapabilityRegistry& capabilityRegistry,
    const RecoverySafetyPolicy& safetyPolicy,
    recoverysuite::disk::IDiskReader* diskReader)
    : capabilityRegistry_(capabilityRegistry),
      safetyPolicy_(safetyPolicy),
      diskReader_(diskReader),
      validator_(capabilityRegistry, safetyPolicy) {
}

bool OutputExporter::exportData(
    bool sourceIsReadOnly,
    bool destinationDiffersFromSource,
    uint64_t startSector,
    uint64_t numSectors,
    bool sourceRangesAreValid,
    bool outputPathIsValid,
    bool arithmeticCannotOverflow,
    bool cancellationStateIsValid,
    const std::map<std::string, std::vector<uint8_t>>& dataToExport) {

    // Validate the operation using our validation framework
    auto report = validator_.validateOperation(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            RecoveryCapability::OUTPUT_EXPORT,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid);

    if (!report.validationPassed()) {
        return false;
    }

    // Check if we have a disk reader (needed for write operations in real implementation)
    if (diskReader_ == nullptr) {
        return false;
    }

    // Validate sector range for the output area
    if (!isSectorRangeValid(startSector, numSectors)) {
        return false;
    }

    // Calculate total space available in sectors
    uint64_t totalBytesAvailable = numSectors * 512; // Assuming 512-byte sectors

    // Calculate total space needed
    uint64_t totalBytesNeeded = 0;
    for (const auto& dataPair : dataToExport) {
        totalBytesNeeded += dataPair.second.size();
    }

    // Check if we have enough space
    if (totalBytesNeeded > totalBytesAvailable) {
        return false; // Not enough space
    }

    // In a real implementation, we would write the data to the disk
    // For now, we'll simulate the write operation
    uint64_t currentOffset = 0;

    for (const auto& dataPair : dataToExport) {
        const std::string& dataName = dataPair.first;
        const std::vector<uint8_t>& data = dataPair.second;

        // Calculate how many sectors we need for this data
        uint64_t sectorsNeeded = calculateRequiredSectors(data.size());

        // Simulate writing the data
        if (!writeSectors(startSector + (currentOffset / 512), sectorsNeeded, data)) {
            return false;
        }

        // Update offset for next data item
        currentOffset += data.size();
    }

    return true;
}

bool OutputExporter::exportDataItem(
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
    uint64_t outputStartSector) {

    // Validate the operation using our validation framework
    auto report = validator_.validateOperation(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            RecoveryCapability::OUTPUT_EXPORT,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid);

    if (!report.validationPassed()) {
        return false;
    }

    // Check if we have a disk reader
    if (diskReader_ == nullptr) {
        return false;
    }

    // Validate sector range
    if (!isSectorRangeValid(outputStartSector, calculateRequiredSectors(data.size()))) {
        return false;
    }

    // In a real implementation, we would write the data to the disk
    // For now, we'll simulate the write operation
    return writeSectors(outputStartSector, calculateRequiredSectors(data.size()), data);
}

bool OutputExporter::isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const {
    // In a real implementation, we would check against the disk's actual size
    // For now, we'll do a basic validation
    if (numSectors == 0) {
        return false;
    }

    // Additional validation would go here
    return true;
}

bool OutputExporter::writeSectors(
    uint64_t startSector,
    uint64_t numSectors,
    const std::vector<uint8_t>& buffer) const {
    // In a real implementation, we would use the disk writer to write sectors
    // For now, we'll simulate writing by just checking the parameters
    // TODO: Actual disk write implementation
    if (diskReader_ == nullptr) {
        return false;
    }

    // In a real implementation, this would call a disk writer interface
    // For simulation, we'll just return true to indicate success
    // (since we're not actually writing to a real disk in this simulation)
    return true;
}

uint64_t OutputExporter::calculateRequiredSectors(uint64_t dataSizeBytes) const {
    // Calculate how many 512-byte sectors are needed for the given data size
    return (dataSizeBytes + 511) / 512; // Round up
}

} // namespace recovery
} // namespace recoverysuite