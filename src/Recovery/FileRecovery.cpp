#include "FileRecovery.h"
#include "source_bytes.h"
#include <limits>

namespace recoverysuite {
namespace recovery {

FileRecovery::FileRecovery(
    const RecoveryCapabilityRegistry& capabilityRegistry,
    const RecoverySafetyPolicy& safetyPolicy,
    recoverysuite::disk::IDiskReader* diskReader)
    : capabilityRegistry_(capabilityRegistry),
      safetyPolicy_(safetyPolicy),
      diskReader_(diskReader),
      validator_(capabilityRegistry, safetyPolicy_) {
}

bool FileRecovery::recoverFiles(
    bool sourceIsReadOnly,
    bool destinationDiffersFromSource,
    uint64_t startSector,
    uint64_t numSectors,
    bool sourceRangesAreValid,
    bool outputPathIsValid,
    bool arithmeticCannotOverflow,
    bool cancellationStateIsValid,
    std::map<std::string, std::vector<uint8_t>>& recoveredFiles) {

    // Clear any previous results
    recoveredFiles.clear();

    // Validate the operation using our validation framework
    auto report = validator_.validateOperation(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            RecoveryCapability::FILE_RECOVERY,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid);

    if (!report.validationPassed()) {
        return false;
    }

    // Directory/cluster-chain reconstruction is unavailable. Never fabricate recovered files.
    (void)startSector; (void)numSectors;
    return false;
}

bool FileRecovery::recoverFile(
    bool sourceIsReadOnly, bool destinationDiffersFromSource, uint64_t startSector, uint64_t numSectors,
    bool sourceRangesAreValid, bool outputPathIsValid, bool arithmeticCannotOverflow,
    bool cancellationStateIsValid, const std::string& fileIdentifier, uint64_t fileStartSector,
    uint64_t fileSizeInBytes, std::vector<uint8_t>& recoveredData) {
    recoveredData.clear();
    const auto report = validator_.validateOperation(sourceIsReadOnly,destinationDiffersFromSource,
        RecoveryCapability::FILE_RECOVERY,sourceRangesAreValid,outputPathIsValid,arithmeticCannotOverflow,cancellationStateIsValid);
    if (!report.validationPassed() || !diskReader_ || fileIdentifier.empty() ||
        !isSectorRangeValid(startSector,numSectors) || fileStartSector < startSector ||
        fileStartSector-startSector >= numSectors) return false;
    const uint64_t sectorSize = diskReader_->getSectorSize(), relative = fileStartSector-startSector;
    if (!sectorSize || relative > std::numeric_limits<uint64_t>::max()/sectorSize) return false;
    return readSourceBytes(diskReader_,startSector,numSectors,relative*sectorSize,fileSizeInBytes,recoveredData);
}

bool FileRecovery::isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const {
    // Validate that we have a disk reader
    if (diskReader_ == nullptr) {
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
        return false; // Overflow would occur
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

} // namespace recovery
} // namespace recoverysuite
