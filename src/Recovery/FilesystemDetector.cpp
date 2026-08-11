#include "FilesystemDetector.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>

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
    std::vector<uint8_t> bootSectorData(512, 0); // Assuming 512-byte sectors

    // In a real implementation, we would read from the disk
    // For now, we'll simulate reading boot sector data
    // TODO: Actual disk read implementation

    // Detect filesystem type from boot sector data
    std::string filesystemType = detectFilesystemType(bootSectorData);

    if (!filesystemType.empty()) {
        detectedFilesystems.push_back(filesystemType);
        return true;
    }

    return false;
}

bool FilesystemDetector::isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const {
    // In a real implementation, we would check against the disk's actual size
    // For now, we'll do a basic validation
    if (numSectors == 0) {
        return false;
    }

    // Additional validation would go here
    return true;
}

std::string FilesystemDetector::detectFilesystemType(const std::vector<uint8_t>& bootSectorData) const {
    // Simple filesystem detection based on boot sector signatures

    // Check for FAT boot sector signature (0x55 0xAA at offset 0x1FE)
    if (bootSectorData.size() >= 512) {
        if (bootSectorData[0x1FE] == 0x55 && bootSectorData[0x1FF] == 0xAA) {
            // Check FAT type based on BPB fields
            uint16_t bytesPerSector = *(uint16_t*)&bootSectorData[0x0B];
            uint8_t sectorsPerCluster = bootSectorData[0x0D];
            uint16_t reservedSectorCount = *(uint16_t*)&bootSectorData[0x0E];
            uint8_t numFATs = bootSectorData[0x10];
            uint16_t rootEntryCount = *(uint16_t*)&bootSectorData[0x11];
            uint16_t totalSectors16 = *(uint16_t*)&bootSectorData[0x13];
            uint32_t totalSectors32 = *(uint32_t*)&bootSectorData[0x20];
            uint32_t FATSize16 = *(uint16_t*)&bootSectorData[0x16];
            uint32_t FATSize32 = *(uint32_t*)&bootSectorData[0x24];

            uint32_t totalSectors = (totalSectors16 != 0) ? totalSectors16 : totalSectors32;
            uint32_t fatSize = (FATSize16 != 0) ? FATSize16 : FATSize32;

            if (bytesPerSector == 512) {
                // Simple heuristic for FAT type
                if (rootEntryCount == 0 && fatSize > 0) {
                    // Likely FAT32
                    return "FAT32";
                } else if (rootEntryCount > 0) {
                    // Likely FAT12 or FAT16
                    // Determine based on total sector count
                    if (totalSectors < 4085) {
                        return "FAT12";
                    } else {
                        return "FAT16";
                    }
                }
            }
            return "FAT";
        }
    }

    // Check for NTFS signature
    if (bootSectorData.size() >= 1024) { // NTFS boot sector is typically 1KB
        // NTFS signature is "NTFS    " at offset 0x3
        if (bootSectorData[0x03] == 'N' &&
            bootSectorData[0x04] == 'T' &&
            bootSectorData[0x05] == 'F' &&
            bootSectorData[0x06] == 'S' &&
            bootSectorData[0x07] == ' ' &&
            bootSectorData[0x08] == ' ' &&
            bootSectorData[0x09] == ' ' &&
            bootSectorData[0x0A] == ' ') {
            return "NTFS";
        }
    }

    // No known filesystem detected
    return "";
}

} // namespace recovery
} // namespace recoverysuite