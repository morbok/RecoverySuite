#include "FileRecovery.h"
#include "MetadataRecovery.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <sstream>

namespace recoverysuite {
namespace recovery {

FileRecovery::FileRecovery(
    const RecoveryCapabilityRegistry& capabilityRegistry,
    const RecoverySafetyPolicy& safetyPolicy,
    recoverysuite::disk::IDiskReader* diskReader)
    : capabilityRegistry_(capabilityRegistry),
      safetyPolicy_(safetyPolicy),
      diskReader_(diskReader),
      validator_(capabilityRegistry, safetyPolicy) {
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

    // Check if we have a disk reader
    if (diskReader_ == nullptr) {
        return false;
    }

    // Validate sector range
    if (!isSectorRangeValid(startSector, numSectors)) {
        return false;
    }

    // Read the boot sector (first sector) to determine filesystem type
    std::vector<uint8_t> bootSectorData(512, 0); // Assuming 512-byte sectors

    // In a real implementation, we would read from the disk
    // For now, we'll simulate reading boot sector data
    // TODO: Actual disk read implementation
    if (!readSectors(startSector, 1, bootSectorData)) {
        return false;
    }

    // Detect filesystem type
    std::string filesystemType = detectFilesystemType(bootSectorData);

    // Recover metadata first (needed for file recovery)
    std::map<std::string, std::vector<uint8_t>> recoveredMetadata;
    bool metadataSuccess = false;

    if (filesystemType == "FAT12" || filesystemType == "FAT16" || filesystemType == "FAT32") {
        // For FAT, we need to recover the FAT tables and directory structure
        MetadataRecovery metadataRecovery(capabilityRegistry_, safetyPolicy_, diskReader_);
        metadataSuccess = metadataRecovery.recoverMetadata(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            startSector,
            numSectors,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid,
            recoveredMetadata);
    } else if (filesystemType == "NTFS") {
        // For NTFS, we need to recover the MFT
        MetadataRecovery metadataRecovery(capabilityRegistry_, safetyPolicy_, diskReader_);
        metadataSuccess = metadataRecovery.recoverMetadata(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            startSector,
            numSectors,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid,
            recoveredMetadata);
    }

    if (!metadataSuccess) {
        return false;
    }

    // Recover files based on filesystem type
    if (filesystemType == "FAT12" || filesystemType == "FAT16" || filesystemType == "FAT32") {
        return recoverFatFiles(bootSectorData, recoveredMetadata, recoveredFiles);
    } else if (filesystemType == "NTFS") {
        return recoverNtfsFiles(bootSectorData, recoveredMetadata, recoveredFiles);
    }

    // Unknown filesystem type
    return false;
}

bool FileRecovery::recoverFile(
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
    std::vector<uint8_t>& recoveredData) {

    // Clear any previous results
    recoveredData.clear();

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

    // Check if we have a disk reader
    if (diskReader_ == nullptr) {
        return false;
    }

    // Validate sector range for the file
    if (!isSectorRangeValid(fileStartSector, (fileSizeInBytes + 511) / 512)) {
        return false;
    }

    // Read the file data
    std::vector<uint8_t> buffer(fileSizeInBytes, 0);

    // In a real implementation, we would read from the disk
    // For now, we'll simulate reading by filling with dummy data
    // TODO: Actual disk read implementation
    if (!readSectors(fileStartSector, (fileSizeInBytes + 511) / 512, buffer)) {
        return false;
    }

    // Copy the recovered data
    recoveredData = buffer;
    return true;
}

bool FileRecovery::isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const {
    // In a real implementation, we would check against the disk's actual size
    // For now, we'll do a basic validation
    if (numSectors == 0) {
        return false;
    }

    // Additional validation would go here
    return true;
}

bool FileRecovery::readSectors(
    uint64_t startSector,
    uint64_t numSectors,
    std::vector<uint8_t>& buffer) const {
    // In a real implementation, we would use the disk reader to read sectors
    // For now, we'll simulate reading by filling with dummy data
    // TODO: Actual disk read implementation
    if (diskReader_ == nullptr) {
        return false;
    }

    // Simulate reading - in reality this would call diskReader_->read()
    // For now, just fill with some pattern to indicate we attempted to read
    std::fill(buffer.begin(), buffer.end(), 0xAA);
    return true;
}

std::string FileRecovery::detectFilesystemType(const std::vector<uint8_t>& bootSectorData) const {
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

bool FileRecovery::recoverFatFiles(
    const std::vector<uint8_t>& bootSectorData,
    const std::map<std::string, std::vector<uint8_t>>& fatMetadata,
    std::map<std::string, std::vector<uint8_t>>& recoveredFiles) const {

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

    // For simplicity, we'll create a few dummy recovered files
    // In a real implementation, we would parse the directory structure and follow FAT chains

    // Create a dummy text file
    std::string dummyText = "This is a recovered text file.\nIt contains multiple lines of text.\n";
    std::vector<uint8_t> textFileData(dummyText.begin(), dummyText.end());
    recoveredFiles["README.TXT"] = textFileData;

    // Create a dummy binary file
    std::vector<uint8_t> binaryData(1024, 0x42); // 1KB of 0x42 bytes
    recoveredFiles["BINARY.BIN"] = binaryData;

    return true;
}

bool FileRecovery::recoverNtfsFiles(
    const std::vector<uint8_t>& bootSectorData,
    const std::map<std::string, std::vector<uint8_t>>& ntfsMetadata,
    std::map<std::string, std::vector<uint8_t>>& recoveredFiles) const {

    if (bootSectorData.size() < 1024) {
        return false;
    }

    // For simplicity, we'll create a few dummy recovered files
    // In a real implementation, we would parse the MFT and extract file data

    // Create a dummy text file
    std::string dummyText = "This is a recovered NTFS text file.\nIt contains multiple lines of text.\n";
    std::vector<uint8_t> textFileData(dummyText.begin(), dummyText.end());
    recoveredFiles["README.TXT"] = textFileData;

    // Create a dummy binary file
    std::vector<uint8_t> binaryData(1024, 0x42); // 1KB of 0x42 bytes
    recoveredFiles["BINARY.BIN"] = binaryData;

    return true;
}

bool FileRecovery::parseFatDirectoryEntry(
    const std::vector<uint8_t>& directoryData,
    size_t entryIndex,
    std::string& fileName,
    uint32_t& firstCluster,
    uint32_t& fileSize) const {

    // Each directory entry is 32 bytes
    if (entryIndex * 32 + 32 > directoryData.size()) {
        return false;
    }

    size_t offset = entryIndex * 32;

    // Check if entry is unused (0x00) or end of directory (0xE5)
    if (directoryData[offset] == 0x00 || directoryData[offset] == 0xE5) {
        return false;
    }

    // Extract filename (8.3 format)
    char nameBuf[9] = {0};
    char extBuf[4] = {0};

    std::memcpy(nameBuf, &directoryData[offset], 8);
    std::memcpy(extBuf, &directoryData[offset + 8], 3);

    // Remove trailing spaces
    std::string name(nameBuf);
    std::string ext(extBuf);

    // Remove trailing spaces
    name.erase(name.find_last_not_of(' ') + 1);
    ext.erase(ext.find_last_not_of(' ') + 1);

    if (!ext.empty()) {
        fileName = name + "." + ext;
    } else {
        fileName = name;
    }

    // Extract first cluster (different for FAT12/FAT16 vs FAT32)
    // For simplicity, we'll assume FAT16 format here
    firstCluster = *(uint16_t*)&directoryData[offset + 0x1A];

    // Extract file size
    fileSize = *(uint32_t*)&directoryData[offset + 0x1C];

    return true;
}

bool FileRecovery::followFatChain(
    const std::vector<uint8_t>& fatData,
    uint16_t bytesPerSector,
    uint8_t sectorsPerCluster,
    uint32_t startCluster,
    uint32_t fileSize,
    std::vector<uint8_t>& fileData) const {

    // For simplicity, we'll just fill with dummy data
    // In a real implementation, we would follow the FAT chain clusters
    fileData.assign(fileSize, 0xFF);
    return true;
}

} // namespace recovery
} // namespace recoverysuite