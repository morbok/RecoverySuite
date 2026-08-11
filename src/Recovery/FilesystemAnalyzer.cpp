#include "FilesystemAnalyzer.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <sstream>

namespace recoverysuite {
namespace recovery {

FilesystemAnalyzer::FilesystemAnalyzer(
    const RecoveryCapabilityRegistry& capabilityRegistry,
    const RecoverySafetyPolicy& safetyPolicy,
    recoverysuite::disk::IDiskReader* diskReader)
    : capabilityRegistry_(capabilityRegistry),
      safetyPolicy_(safetyPolicy),
      diskReader_(diskReader),
      validator_(capabilityRegistry, safetyPolicy) {
}

bool FilesystemAnalyzer::analyzeFilesystem(
    bool sourceIsReadOnly,
    bool destinationDiffersFromSource,
    uint64_t startSector,
    uint64_t numSectors,
    bool sourceRangesAreValid,
    bool outputPathIsValid,
    bool arithmeticCannotOverflow,
    bool cancellationStateIsValid,
    std::map<std::string, std::string>& analysisResults) {

    // Clear any previous results
    analysisResults.clear();

    // Validate the operation using our validation framework
    auto report = validator_.validateOperation(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            RecoveryCapability::FILESYSTEM_ANALYSIS,
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

    // Read the boot sector (first sector)
    std::vector<uint8_t> bootSectorData(512, 0); // Assuming 512-byte sectors

    // In a real implementation, we would read from the disk
    // For now, we'll simulate reading boot sector data
    // TODO: Actual disk read implementation
    if (!readSectors(startSector, 1, bootSectorData)) {
        return false;
    }

    // Analyze based on filesystem type detected
    std::string filesystemType = detectFilesystemType(bootSectorData);

    if (filesystemType == "FAT12" || filesystemType == "FAT16" || filesystemType == "FAT32") {
        return analyzeFatFilesystem(bootSectorData, analysisResults);
    } else if (filesystemType == "NTFS") {
        return analyzeNtfsFilesystem(bootSectorData, analysisResults);
    }

    // Unknown filesystem type
    return false;
}

bool FilesystemAnalyzer::getFilesystemInfo(
    bool sourceIsReadOnly,
    bool destinationDiffersFromSource,
    uint64_t startSector,
    uint64_t numSectors,
    bool sourceRangesAreValid,
    bool outputPathIsValid,
    bool arithmeticCannotOverflow,
    bool cancellationStateIsValid,
    const std::string& filesystemType,
    std::map<std::string, std::string>& info) {

    // Clear any previous results
    info.clear();

    // Validate the operation using our validation framework
    auto report = validator_.validateOperation(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            RecoveryCapability::FILESYSTEM_ANALYSIS,
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

    // Read the boot sector (first sector)
    std::vector<uint8_t> bootSectorData(512, 0); // Assuming 512-byte sectors

    // In a real implementation, we would read from the disk
    // For now, we'll simulate reading boot sector data
    // TODO: Actual disk read implementation
    if (!readSectors(startSector, 1, bootSectorData)) {
        return false;
    }

    // Analyze based on filesystem type
    if (filesystemType == "FAT12" || filesystemType == "FAT16" || filesystemType == "FAT32") {
        return analyzeFatFilesystem(bootSectorData, info);
    } else if (filesystemType == "NTFS") {
        return analyzeNtfsFilesystem(bootSectorData, info);
    }

    // Unknown filesystem type
    return false;
}

bool FilesystemAnalyzer::isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const {
    // In a real implementation, we would check against the disk's actual size
    // For now, we'll do a basic validation
    if (numSectors == 0) {
        return false;
    }

    // Additional validation would go here
    return true;
}

bool FilesystemAnalyzer::readSectors(
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

std::string FilesystemAnalyzer::detectFilesystemType(const std::vector<uint8_t>& bootSectorData) const {
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

bool FilesystemAnalyzer::analyzeFatFilesystem(
    const std::vector<uint8_t>& bootSectorData,
    std::map<std::string, std::string>& analysisResults) const {

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

    // Populate analysis results
    analysisResults["Filesystem Type"] = fatType;
    analysisResults["Bytes Per Sector"] = std::to_string(bytesPerSector);
    analysisResults["Sectors Per Cluster"] = std::to_string(sectorsPerCluster);
    analysisResults["Reserved Sector Count"] = std::to_string(reservedSectorCount);
    analysisResults["Number of FATs"] = std::to_string(numFATs);
    analysisResults["Root Entry Count"] = std::to_string(rootEntryCount);
    analysisResults["Total Sectors (16-bit)"] = std::to_string(totalSectors16);
    analysisResults["Total Sectors (32-bit)"] = std::to_string(totalSectors32);
    analysisResults["FAT Size (16-bit)"] = std::to_string(fatSize16);
    analysisResults["FAT Size (32-bit)"] = std::to_string(fatSize32);
    analysisResults["Total Sectors"] = std::to_string(totalSectors);
    analysisResults["FAT Size"] = std::to_string(fatSize);

    // Calculate derived values
    uint32_t rootDirSectors = ((rootEntryCount * 32) + (bytesPerSector - 1)) / bytesPerSector;
    uint32_t firstDataSector = reservedSectorCount + (numFATs * fatSize) + rootDirSectors;
    uint32_t clusterCount = (totalSectors - firstDataSector) / sectorsPerCluster;

    analysisResults["Root Directory Sectors"] = std::to_string(rootDirSectors);
    analysisResults["First Data Sector"] = std::to_string(firstDataSector);
    analysisResults["Cluster Count"] = std::to_string(clusterCount);

    return true;
}

bool FilesystemAnalyzer::analyzeNtfsFilesystem(
    const std::vector<uint8_t>& bootSectorData,
    std::map<std::string, std::string>& analysisResults) const {

    if (bootSectorData.size() < 1024) {
        return false;
    }

    // NTFS boot sector analysis (simplified)
    // In a real implementation, we would parse the NTFS boot sector properly

    analysisResults["Filesystem Type"] = "NTFS";
    analysisResults["Version"] = "NTFS 3.1"; // Most common modern version

    // Extract some basic fields from NTFS boot sector
    // Bytes per sector
    uint16_t bytesPerSector = *(uint16_t*)&bootSectorData[0x0B];
    analysisResults["Bytes Per Sector"] = std::to_string(bytesPerSector);

    // Sectors per cluster
    uint8_t sectorsPerCluster = bootSectorData[0x0D];
    analysisResults["Sectors Per Cluster"] = std::to_string(sectorsPerCluster);

    // MFT start cluster
    int64_t mftStartCluster = *(int64_t*)&bootSectorData[0x30];
    analysisResults["MFT Start Cluster"] = std::to_string(mftStartCluster);

    // MFT mirror start cluster
    int64_t mftMirrorStartCluster = *(int64_t*)&bootSectorData[0x38];
    analysisResults["MFT Mirror Start Cluster"] = std::to_string(mftMirrorStartCluster);

    // Bytes per file record segment
    int8_t bytesPerFileRecordSegment = bootSectorData[0x40];
    uint32_t fileRecordSegmentSize = (bytesPerFileRecordSegment > 0)
        ? bytesPerFileRecordSegment
        : (1 << (-bytesPerFileRecordSegment));
    analysisResults["File Record Segment Size"] = std::to_string(fileRecordSegmentSize);

    // Bytes per index block
    int8_t bytesPerIndexBlock = bootSectorData[0x44];
    uint32_t indexBlockSize = (bytesPerIndexBlock > 0)
        ? bytesPerIndexBlock
        : (1 << (-bytesPerIndexBlock));
    analysisResults["Index Block Size"] = std::to_string(indexBlockSize);

    // Volume serial number
    uint64_t volumeSerial = *(uint64_t*)&bootSectorData[0x48];
    std::stringstream ss;
    ss << std::hex << volumeSerial;
    analysisResults["Volume Serial Number"] = ss.str();

    // Checksum
    uint16_t checksum = *(uint16_t*)&bootSectorData[0x50];
    analysisResults["Boot Sector Checksum"] = std::to_string(checksum);

    return true;
}

} // namespace recovery
} // namespace recoverysuite