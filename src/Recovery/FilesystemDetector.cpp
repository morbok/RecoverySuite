#include "FilesystemDetector.h"
#include "BootSectorDetection.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <vector>
#include <cstddef> // for std::byte

namespace recoverysuite {
namespace recovery {

FilesystemDetector::FilesystemDetector(
    const RecoveryCapabilityRegistry& capabilityRegistry,
    const RecoverySafetyPolicy& safetyPolicy,
    recoverysuite::disk::IDiskReader* diskReader)
    : capabilityRegistry_(capabilityRegistry),
      safetyPolicy_(safetyPolicy),
      diskReader_(diskReader),
      validator_(capabilityRegistry, safetyPolicy) {
}

bool FilesystemDetector::detectFilesystems(
    bool sourceIsReadOnly,
    bool destinationDiffersFromSource,
    uint64_t startSector,
    uint64_t numSectors,
    bool sourceRangesAreValid,
    bool outputPathIsValid,
    bool arithmeticCannotOverflow,
    bool cancellationStateIsValid,
    std::vector<std::string>& detectedFilesystems) {

    // Clear any previous results
    detectedFilesystems.clear();

    // Validate the operation using our validation framework
    // Perform safety validation
    auto report = validator_.validateOperation(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            RecoveryCapability::FILESYSTEM_DETECTION,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid);

    if (!report.validationPassed()) {
        // For simplicity, we'll just return false if validation fails
        // In a real implementation, we might want to report these errors
        return false;
    }

    // Check if we have a disk reader
    if (diskReader_ == nullptr) {
        return false;
    }

    // Validate sector range
    if (!isSectorRangeValid(startSector, numSectors)) {
        return false;
    }

    // Read the boot sector (first sector)
    // Get actual sector size from disk reader
    uint32_t sectorSize = diskReader_->getSectorSize();
    if (sectorSize == 0) {
        return false; // Invalid sector size
    }

    std::vector<uint8_t> bootSectorData(sectorSize, 0);

    // Actual disk read implementation
    if (diskReader_ == nullptr) {
        return false;
    }

    // Create vector of std::byte for the disk reader interface
    std::vector<std::byte> byteBuffer(sectorSize);

    // Read sectors from disk
    if (!diskReader_->readSectors(startSector, 1, byteBuffer)) {
        return false;
    }

    // Copy data from std::byte vector to uint8_t vector
    std::copy(byteBuffer.begin(), byteBuffer.end(),
              reinterpret_cast<std::byte*>(bootSectorData.data()));

    // Detect filesystem type from boot sector data
    std::string filesystemType = detectFilesystemType(bootSectorData);

    if (!filesystemType.empty()) {
        detectedFilesystems.push_back(filesystemType);
        return true;
    }

    return false;
}

bool FilesystemDetector::isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const {
    if (!diskReader_ || !diskReader_->isOpen() || numSectors == 0) return false;
    const auto totalSectors = diskReader_->getTotalSectors();
    return startSector < totalSectors && numSectors <= totalSectors - startSector;
}

std::string FilesystemDetector::detectFilesystemType(const std::vector<uint8_t>& bootSectorData) const {
    return detectBootSectorType(bootSectorData);
}

} // namespace recovery
} // namespace recoverysuite