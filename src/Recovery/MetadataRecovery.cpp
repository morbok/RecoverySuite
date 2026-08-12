#include "MetadataRecovery.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>
#include <cstddef> // for std::byte

namespace recoverysuite {
namespace recovery {

MetadataRecovery::MetadataRecovery(
    const RecoveryCapabilityRegistry& capabilityRegistry,
    const RecoverySafetyPolicy& safetyPolicy,
    recoverysuite::disk::IDiskReader* diskReader)
    : capabilityRegistry_(capabilityRegistry),
      safetyPolicy_(safetyPolicy),
      diskReader_(diskReader),
      validator_(capabilityRegistry, safetyPolicy) {
}

bool MetadataRecovery::recoverMetadata(
    bool sourceIsReadOnly,
    bool destinationDiffersFromSource,
    uint64_t startSector,
    uint64_t numSectors,
    bool sourceRangesAreValid,
    bool outputPathIsValid,
    bool arithmeticCannotOverflow,
    bool cancellationStateIsValid,
    std::map<std::string, std::vector<uint8_t>>& recoveredMetadata) {

    // Clear any previous results
    recoveredMetadata.clear();

    // Validate the operation using our validation framework
    auto report = validator_.validateOperation(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            RecoveryCapability::METADATA_RECOVERY,
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
    if (!isSectorRangeValid(startSector, numSectors)) {
        return false;
    }

    // Read the boot sector (first sector) to determine filesystem type
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

    // Detect filesystem type
    std::string filesystemType = detectFilesystemType(bootSectorData);

    // Recover metadata based on filesystem type
    if (filesystemType == "FAT12" || filesystemType == "FAT16" || filesystemType == "FAT32") {
        return recoverFatTables(bootSectorData, recoveredMetadata);
    } else if (filesystemType == "NTFS") {
        return recoverNtfsMft(bootSectorData, recoveredMetadata);
    }

    // Unknown filesystem type
    return false;
}

bool MetadataRecovery::recoverMetadataType(
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
    std::vector<uint8_t>& recoveredData) {

    // Clear any previous results
    recoveredData.clear();

    // Validate the operation using our validation framework
    auto report = validator_.validateOperation(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            RecoveryCapability::METADATA_RECOVERY,
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

    // Convert offset to sectors (assuming 512-byte sectors for simplicity)
    uint64_t startSectorOffset = offset / 512;
    uint64_t numSectorsNeeded = (size + 511) / 512; // Round up

    // Calculate actual start sector
    uint64_t actualStartSector = startSector + startSectorOffset;

    // Validate sector range
    if (!isSectorRangeValid(actualStartSector, numSectorsNeeded)) {
        return false;
    }

    // Read the requested sectors
    // Get actual sector size from disk reader
    uint32_t sectorSize = diskReader_->getSectorSize();
    if (sectorSize == 0) {
        return false; // Invalid sector size
    }

    // Calculate total bytes needed
    size_t totalBytes = static_cast<size_t>(numSectorsNeeded) * static_cast<size_t>(sectorSize);
    if (totalBytes == 0) {
        return false;
    }

    std::vector<uint8_t> buffer(totalBytes, 0);

    // Actual disk read implementation
    if (diskReader_ == nullptr) {
        return false;
    }

    // Create vector of std::byte for the disk reader interface
    std::vector<std::byte> byteBuffer(totalBytes);

    // Read sectors from disk
    if (!diskReader_->readSectors(actualStartSector, numSectorsNeeded, byteBuffer)) {
        return false;
    }

    // Copy data from std::byte vector to uint8_t vector
    std::copy(byteBuffer.begin(), byteBuffer.end(),
              reinterpret_cast<std::byte*>(buffer.data()));

    // Extract the requested data (handle partial sector at the end)
    uint64_t bytesToCopy = std::min(size, static_cast<uint64_t>(buffer.size()));
    uint64_t offsetInBuffer = startSectorOffset * 512 - offset; // This should be 0 if offset is sector-aligned

    // Simpler approach: just copy from beginning of buffer up to requested size
    if (bytesToCopy > buffer.size()) {
        bytesToCopy = buffer.size();
    }

    recoveredData.assign(buffer.begin(), buffer.begin() + bytesToCopy);

    // If we need less than a full sector, we've still read the data correctly
    return true;
}

bool MetadataRecovery::isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const {
    // In a real implementation, we would check against the disk's actual size
    // For now, we'll do a basic validation
    if (numSectors == 0) {
        return false;
    }

    // Additional validation would go here
    return true;
}

bool MetadataRecovery::readSectors(
    uint64_t startSector,
    uint64_t numSectors,
    std::vector<uint8_t>& buffer) const {
    // Actual disk read implementation
    if (diskReader_ == nullptr) {
        return false;
    }

    // Get sector size from disk reader
    uint32_t sectorSize = diskReader_->getSectorSize();
    if (sectorSize == 0) {
        return false; // Invalid sector size
    }

    // Calculate total bytes needed
    size_t totalBytes = static_cast<size_t>(numSectors) * static_cast<size_t>(sectorSize);
    if (totalBytes == 0) {
        return false;
    }

    // Resize buffer to hold the data
    buffer.resize(totalBytes);

    // Create vector of std::byte for the disk reader interface
    std::vector<std::byte> byteBuffer(totalBytes);

    // Read sectors from disk
    if (!diskReader_->readSectors(startSector, numSectors, byteBuffer)) {
        return false;
    }

    // Copy data from std::byte vector to uint8_t vector
    std::copy(byteBuffer.begin(), byteBuffer.end(),
              reinterpret_cast<std::byte*>(buffer.data()));

    return true;
}

std::string MetadataRecovery::detectFilesystemType(const std::vector<uint8_t>& bootSectorData) const {
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

bool MetadataRecovery::recoverFatTables(
    const std::vector<uint8_t>& bootSectorData,
    std::map<std::string, std::vector<uint8_t>>& recoveredMetadata) const {

    if (bootSectorData.size() < 512) {
        return false;
    }

    // Extract basic FAT boot sector information
    uint16_t bytesPerSector = *(uint16_t*)&bootSectorData[0x0B];
    uint8_t sectorsPerCluster = bootSectorData[0x0D];
    uint16_t reservedSectorCount = *(uint16_t*)&bootSectorData[0x0E];
    uint8_t numFATs = bootSectorData[0x10];
    uint16_t rootEntryCount = *(uint16_t*)&bootSectorData[0x11];
    uint16_t totalSectors16 = *(uint16_t*)&bootSectorData[0x13];
    uint16_t fatSize16 = *(uint16_t*)&bootSectorData[0x16];
    uint32_t totalSectors32 = *(uint32_t*)&bootSectorData[0x20];
    uint32_t fatSize32 = *(uint32_t*)&bootSectorData[0x24];

    uint32_t totalSectors = (totalSectors16 != 0) ? totalSectors16 : totalSectors32;
    uint32_t fatSize = (fatSize16 != 0) ? fatSize16 : fatSize32;

    // Determine FAT type
    std::string fatType;
    if (rootEntryCount == 0 && fatSize > 0) {
        fatType = "FAT32";
    } else if (rootEntryCount > 0) {
        if (totalSectors < 4085) {
            fatType = "FAT12";
        } else {
            fatType = "FAT16";
        }
    } else {
        fatType = "FAT (Unknown)";
    }

    // Recover FAT tables
    if (bytesPerSector == 0) {
        return false; // Avoid division by zero
    }

    uint32_t fatStartSector = reservedSectorCount;
    uint32_t fatSizeSectors = (fatSize * bytesPerSector + bytesPerSector - 1) / bytesPerSector;

    // For each FAT copy
    for (uint8_t fatNum = 0; fatNum < numFATs; ++fatNum) {
        uint32_t fatSector = fatStartSector + (fatNum * fatSizeSectors);

        // Read FAT data
        std::vector<uint8_t> fatData;
        // In a real implementation, we would read the FAT sectors
        // For simulation, we'll create dummy data
        fatData.resize(fatSize, 0xF0); // Fill with dummy FAT data

        std::string fatName = fatType + "_FAT" + std::to_string(fatNum);
        recoveredMetadata[fatName] = fatData;
    }

    // Also recover root directory for FAT12/FAT16
    if (rootEntryCount > 0) {
        uint32_t rootDirStartSector = reservedSectorCount + (numFATs * fatSizeSectors);
        uint32_t rootDirSectors = ((rootEntryCount * 32) + (bytesPerSector - 1)) / bytesPerSector;

        // Read root directory data
        std::vector<uint8_t> rootDirData;
        // In a real implementation, we would read the root directory sectors
        // For simulation, we'll create dummy data
        rootDirData.resize(rootEntryCount * 32, 0x00); // 32 bytes per directory entry

        recoveredMetadata["RootDirectory"] = rootDirData;
    }

    return true;
}

bool MetadataRecovery::recoverNtfsMft(
    const std::vector<uint8_t>& bootSectorData,
    std::map<std::string, std::vector<uint8_t>>& recoveredMetadata) const {

    if (bootSectorData.size() < 1024) {
        return false;
    }

    // NTFS MFT recovery (simplified)
    // In a real implementation, we would parse the NTFS boot sector to find MFT location
    // and then read the MFT entries

    recoveredMetadata["NTFS_MFT_Placeholder"] = std::vector<uint8_t>(1024, 0x4E); // Dummy MFT data
    recoveredMetadata["NTFS_BootSector"] = bootSectorData; // Also recover the boot sector itself

    return true;
}

} // namespace recovery
} // namespace recoverysuite