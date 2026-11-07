#include "FilesystemAnalyzer.h"
#include "BootSectorDetection.h"
#include "source_bytes.h"
#include <limits>

namespace recoverysuite {
namespace recovery {

FilesystemAnalyzer::FilesystemAnalyzer(
    const RecoveryCapabilityRegistry& capabilityRegistry,
    const RecoverySafetyPolicy& safetyPolicy,
    recoverysuite::disk::IDiskReader* diskReader)
    : capabilityRegistry_(capabilityRegistry),
      safetyPolicy_(safetyPolicy),
      diskReader_(diskReader),
      validator_(capabilityRegistry, safetyPolicy_),
      startSector_(0) {
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

    // Store the start sector for use in helper methods
    startSector_ = startSector;

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

    std::vector<uint8_t> bootSectorData;
    if (!readSourceBytes(diskReader_,startSector,numSectors,0,diskReader_->getSectorSize(),bootSectorData)) return false;

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

    // Store the start sector for use in helper methods
    startSector_ = startSector;

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

    std::vector<uint8_t> bootSectorData;
    if (!readSourceBytes(diskReader_,startSector,numSectors,0,diskReader_->getSectorSize(),bootSectorData) ||
        detectFilesystemType(bootSectorData) != filesystemType) return false;

    // Analyze based on filesystem type
    if (filesystemType == "FAT12" || filesystemType == "FAT16" || filesystemType == "FAT32") {
        return analyzeFatFilesystem(bootSectorData, info);
    } else if (filesystemType == "NTFS") {
        return analyzeNtfsFilesystem(bootSectorData, info);
    }

    // Unknown filesystem type
    return false;
}

bool FilesystemAnalyzer::analyzeFatFromBootSector(
    const std::vector<uint8_t>& bootSectorData,
    std::map<std::string, std::string>& analysisResults) const {
    // Analyze FAT filesystem using only boot sector data
    // This is useful when we can't read the full FAT structures

    const auto actualType = detectBootSectorType(bootSectorData);
    if (actualType != "FAT12" && actualType != "FAT16" && actualType != "FAT32") return false;

    // Check for FAT boot sector signature (0x55 0xAA at offset 0x1FE)
    if (bootSectorData[0x1FE] != 0x55 || bootSectorData[0x1FF] != 0xAA) {
        return false; // Not a FAT boot sector
    }

    // Extract FAT BPB (BIOS Parameter Block) fields
    uint16_t bytesPerSector = sourceLe16(bootSectorData,0x0B);
    uint8_t sectorsPerCluster = bootSectorData[0x0D];
    uint16_t reservedSectorCount = sourceLe16(bootSectorData,0x0E);
    uint8_t numFATs = bootSectorData[0x10];
    uint16_t rootEntryCount = sourceLe16(bootSectorData,0x11);
    uint16_t totalSectors16 = sourceLe16(bootSectorData,0x13);
    uint32_t totalSectors32 = sourceLe32(bootSectorData,0x20);
    uint16_t fatSize16 = sourceLe16(bootSectorData,0x16);
    uint32_t fatSize32 = sourceLe32(bootSectorData,0x24);

    const auto& fatType = actualType;

    // Populate analysis results from boot sector data
    analysisResults["Filesystem Type"] = fatType;
    analysisResults["Version"] = "unknown";
    analysisResults["Bytes Per Sector"] = std::to_string(bytesPerSector);
    analysisResults["Sectors Per Cluster"] = std::to_string(sectorsPerCluster);
    analysisResults["Reserved Sector Count"] = std::to_string(reservedSectorCount);
    analysisResults["Number of FATs"] = std::to_string(numFATs);
    analysisResults["Root Entry Count"] = std::to_string(rootEntryCount);
    analysisResults["Total Sectors (16-bit)"] = std::to_string(totalSectors16);
    analysisResults["Total Sectors (32-bit)"] = std::to_string(totalSectors32);
    analysisResults["FAT Size (16-bit)"] = std::to_string(fatSize16);
    analysisResults["FAT Size (32-bit)"] = std::to_string(fatSize32);

    // Calculate FAT size value for display
    uint32_t fatSize = (fatSize16 != 0) ? fatSize16 : fatSize32;
    analysisResults["FAT Size"] = std::to_string(fatSize);

    // Total sectors
    uint32_t totalSectors = (totalSectors16 != 0) ? totalSectors16 : totalSectors32;
    analysisResults["Total Sectors"] = std::to_string(totalSectors);

    // Calculate derived values
    uint32_t rootDirSectors = ((rootEntryCount * 32) + (bytesPerSector - 1)) / bytesPerSector;
    uint32_t firstDataSector = reservedSectorCount + (numFATs * fatSize) + rootDirSectors;
    uint32_t clusterCount = (totalSectors > firstDataSector) ?
                            (totalSectors - firstDataSector) / sectorsPerCluster : 0;

    analysisResults["Root Directory Sectors"] = std::to_string(rootDirSectors);
    analysisResults["First Data Sector"] = std::to_string(firstDataSector);
    analysisResults["Cluster Count"] = std::to_string(clusterCount);

    // Extended BPB fields are reported only when their signature declares them present.
    const size_t serialOffset = actualType == "FAT32" ? 67 : 39;
    const size_t labelOffset = actualType == "FAT32" ? 71 : 43;
    const bool extended = bootSectorData[serialOffset-1] == 0x29;
    analysisResults["Volume Serial Number"] = extended ? std::to_string(sourceLe32(bootSectorData,serialOffset)) : "unknown";
    std::string label = "unknown";
    if (extended) {
        label.clear();
        for (size_t i=labelOffset;i<labelOffset+11;++i) {
            const auto value=bootSectorData[i];label.push_back(value>=32 && value<=126 ? static_cast<char>(value) : '?');
        }
        while (!label.empty() && label.back()==' ') label.pop_back();
    }
    analysisResults["Volume Label"] = label;
    analysisResults["Total Size"] = std::to_string(uint64_t{totalSectors}*bytesPerSector);
    if (totalSectors > diskReader_->getTotalSectors()-startSector_)
        analysisResults["Geometry Warning"] = "Declared FAT volume extends beyond the source";
    analysisResults["Free Space"] = "unknown"; analysisResults["Used Space"] = "unknown";
    analysisResults["Analysis Scope"] = "Boot-sector fields only; allocation tables and free space were not evaluated";

    return true;
}

bool FilesystemAnalyzer::analyzeNtfsFromBootSector(
    const std::vector<uint8_t>& bootSectorData,
    std::map<std::string, std::string>& analysisResults) const {
    // Analyze NTFS filesystem using only boot sector data
    // This is useful when we can't read the full NTFS structures

    if (detectBootSectorType(bootSectorData) != "NTFS") return false;

    // Check for NTFS signature
    if (bootSectorData[0x03] == 'N' &&
        bootSectorData[0x04] == 'T' &&
        bootSectorData[0x05] == 'F' &&
        bootSectorData[0x06] == 'S' &&
        bootSectorData[0x07] == ' ' &&
        bootSectorData[0x08] == ' ' &&
        bootSectorData[0x09] == ' ' &&
        bootSectorData[0x0A] == ' ') {

        // Extract basic NTFS information from boot sector
        // Note: Full NTFS structure is complex, so we'll extract what we can from boot sector

        analysisResults["Filesystem Type"] = "NTFS";
        analysisResults["Version"] = "unknown";

        // Bytes per sector (offset 0x0B-0x0C)
        uint16_t bytesPerSector = sourceLe16(bootSectorData,0x0B);
        analysisResults["Bytes Per Sector"] = std::to_string(bytesPerSector);

        // We can't easily get sectors per cluster from boot sector alone without more complex parsing
        analysisResults["Sectors Per Cluster"] = std::to_string(bootSectorData[13]);

        const uint64_t sectors = sourceLe32(bootSectorData,40) | (sourceLe32(bootSectorData,44)<<32);
        const uint64_t serial = sourceLe32(bootSectorData,72) | (sourceLe32(bootSectorData,76)<<32);
        analysisResults["Volume Serial Number"] = std::to_string(serial);
        analysisResults["Volume Label"] = "unknown";
        analysisResults["Total Sectors"] = std::to_string(sectors);
        analysisResults["Total Size"] = sectors && sectors <= std::numeric_limits<uint64_t>::max()/bytesPerSector
            ? std::to_string(sectors*bytesPerSector) : "unknown";
        if (!sectors || sectors > std::numeric_limits<uint64_t>::max()/bytesPerSector)
            analysisResults["Geometry Warning"] = "Declared NTFS volume size is zero or overflows 64-bit bytes";
        else if (sectors > diskReader_->getTotalSectors()-startSector_)
            analysisResults["Geometry Warning"] = "Declared NTFS volume extends beyond the source";
        analysisResults["Free Space"] = "unknown"; analysisResults["Used Space"] = "unknown";
        analysisResults["Analysis Scope"] = "Boot-sector fields only; MFT, allocation bitmap and filesystem version were not evaluated";

        return true;
    }

    return false; // Not an NTFS boot sector
}

bool FilesystemAnalyzer::isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const {
    if (!diskReader_ || !diskReader_->isOpen() || numSectors == 0) return false;
    const auto total = diskReader_->getTotalSectors();
    return startSector < total && numSectors <= total - startSector;
}

std::string FilesystemAnalyzer::detectFilesystemType(const std::vector<uint8_t>& bootSectorData) const {
    return detectBootSectorType(bootSectorData);
}

bool FilesystemAnalyzer::analyzeFatFilesystem(const std::vector<uint8_t>& data,
    std::map<std::string,std::string>& results) const { return analyzeFatFromBootSector(data,results); }
bool FilesystemAnalyzer::analyzeNtfsFilesystem(const std::vector<uint8_t>& data,
    std::map<std::string,std::string>& results) const { return analyzeNtfsFromBootSector(data,results); }
} // namespace recovery
} // namespace recoverysuite
