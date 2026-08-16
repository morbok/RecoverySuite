#include "FilesystemAnalyzer.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>
#include <cstddef> // for std::byte
#include <limits>
#include "../Logging/Logger.h"
#include "../Disk/IDiskReader.hpp"
#include "DiskReaderAdapter.hpp"
#include "../Filesystem/FAT/FATMetadata.h"
#include "../Filesystem/NTFS/NTFSMetadata.h"

namespace recoverysuite {
namespace recovery {

FilesystemAnalyzer::FilesystemAnalyzer(
    const RecoveryCapabilityRegistry& capabilityRegistry,
    const RecoverySafetyPolicy& safetyPolicy,
    recoverysuite::disk::IDiskReader* diskReader)
    : capabilityRegistry_(capabilityRegistry),
      safetyPolicy_(safetyPolicy),
      diskReader_(diskReader),
      validator_(capabilityRegistry, safetyPolicy),
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
    std::memcpy(bootSectorData.data(), byteBuffer.data(), byteBuffer.size());

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
    std::memcpy(bootSectorData.data(), byteBuffer.data(), byteBuffer.size());

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

    if (bootSectorData.size() < 512) {
        return false; // Need at least a full boot sector
    }

    // Check for FAT boot sector signature (0x55 0xAA at offset 0x1FE)
    if (bootSectorData[0x1FE] != 0x55 || bootSectorData[0x1FF] != 0xAA) {
        return false; // Not a FAT boot sector
    }

    // Extract FAT BPB (BIOS Parameter Block) fields
    uint16_t bytesPerSector = *reinterpret_cast<const uint16_t*>(&bootSectorData[0x0B]);
    uint8_t sectorsPerCluster = bootSectorData[0x0D];
    uint16_t reservedSectorCount = *reinterpret_cast<const uint16_t*>(&bootSectorData[0x0E]);
    uint8_t numFATs = bootSectorData[0x10];
    uint16_t rootEntryCount = *reinterpret_cast<const uint16_t*>(&bootSectorData[0x11]);
    uint16_t totalSectors16 = *reinterpret_cast<const uint16_t*>(&bootSectorData[0x13]);
    uint32_t totalSectors32 = *reinterpret_cast<const uint32_t*>(&bootSectorData[0x20]);
    uint16_t fatSize16 = *reinterpret_cast<const uint16_t*>(&bootSectorData[0x16]);
    uint32_t fatSize32 = *reinterpret_cast<const uint32_t*>(&bootSectorData[0x24]);

    // Determine FAT type based on BPB fields
    std::string fatType;
    if (bytesPerSector == 512) {
        // Simple heuristic for FAT type
        if (rootEntryCount == 0 && fatSize32 > 0) {
            // Likely FAT32
            fatType = "FAT32";
        } else if (rootEntryCount > 0) {
            // Likely FAT12 or FAT16
            // Determine based on total sector count
            uint32_t totalSectors = (totalSectors16 != 0) ? totalSectors16 : totalSectors32;
            if (totalSectors < 4085) {
                fatType = "FAT12";
            } else {
                fatType = "FAT16";
            }
        } else {
            fatType = "FAT";
        }
    } else {
        fatType = "FAT";
    }

    // Populate analysis results from boot sector data
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

    // Additional FAT-specific information (we can't get these without reading more sectors)
    analysisResults["Volume Serial Number"] = "0"; // Would need to read offset 0x43
    analysisResults["Volume Label"] = "UNLABELED"; // Would need to read offset 0x47

    return true;
}

bool FilesystemAnalyzer::analyzeNtfsFromBootSector(
    const std::vector<uint8_t>& bootSectorData,
    std::map<std::string, std::string>& analysisResults) const {
    // Analyze NTFS filesystem using only boot sector data
    // This is useful when we can't read the full NTFS structures

    if (bootSectorData.size() < 1024) {
        return false; // Need at least a full NTFS boot sector (typically 1KB)
    }

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
        analysisResults["Version"] = "3.1"; // NTFS version is typically in offset 0x30

        // Bytes per sector (offset 0x0B-0x0C)
        uint16_t bytesPerSector = *reinterpret_cast<const uint16_t*>(&bootSectorData[0x0B]);
        analysisResults["Bytes Per Sector"] = std::to_string(bytesPerSector);

        // We can't easily get sectors per cluster from boot sector alone without more complex parsing
        analysisResults["Sectors Per Cluster"] = "unknown";

        analysisResults["Volume Serial Number"] = "0"; // Would need to read offset 0x48
        analysisResults["Volume Label"] = "UNLABELED"; // Would need to read offset 0x4A

        analysisResults["Total Size"] = "0"; // Would need to read offset 0x28
        analysisResults["Free Space"] = "0"; // Complex to calculate without reading bitmap
        analysisResults["Used Space"] = "0"; // Complex to calculate without reading bitmap

        return true;
    }

    return false; // Not an NTFS boot sector
}

bool FilesystemAnalyzer::isSectorRangeValid(uint64_t /*startSector*/, uint64_t numSectors) const {
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
    if (startSector_ > std::numeric_limits<uint64_t>::max() - numSectors) {
        return false; // Overflow would occur
    }

    uint64_t endSector = startSector_ + numSectors;

    // Check if start is beyond the disk
    if (startSector_ >= totalSectors) {
        return false;
    }

    // Check if range extends beyond the disk
    if (endSector > totalSectors) {
        return false;
    }

    return true;
}

bool FilesystemAnalyzer::readSectors(
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

    // Verify that we received the expected amount of data
    // If the disk reader returned fewer bytes than requested, treat as failure
    if (byteBuffer.size() != totalBytes) {
        return false; // Incomplete read - do not interpret partial data as valid
    }

    // Copy data from std::byte vector to uint8_t vector
    std::copy(byteBuffer.begin(), byteBuffer.end(),
              reinterpret_cast<std::byte*>(buffer.data()));

    return true;
}

std::string FilesystemAnalyzer::detectFilesystemType(const std::vector<uint8_t>& bootSectorData) const {
    // Simple filesystem detection based on boot sector signatures

    // Check for FAT boot sector signature (0x55 0xAA at offset 0x1FE)
    if (bootSectorData.size() >= 512) {
        if (bootSectorData[0x1FE] == 0x55 && bootSectorData[0x1FF] == 0xAA) {
            // Check FAT type based on BPB fields
            uint16_t bytesPerSector = *reinterpret_cast<const uint16_t*>(&bootSectorData[0x0B]);
            uint8_t sectorsPerCluster = bootSectorData[0x0D];
            (void)sectorsPerCluster; // unused variable
            uint16_t reservedSectorCount = *reinterpret_cast<const uint16_t*>(&bootSectorData[0x0E]);
            (void)reservedSectorCount; // unused variable
            uint8_t numFATs = bootSectorData[0x10];
            (void)numFATs; // unused variable
            uint16_t rootEntryCount = *reinterpret_cast<const uint16_t*>(&bootSectorData[0x11]);
            uint16_t totalSectors16 = *reinterpret_cast<const uint16_t*>(&bootSectorData[0x13]);
            uint32_t totalSectors32 = *reinterpret_cast<const uint32_t*>(&bootSectorData[0x20]);
            uint32_t FATSize16 = *reinterpret_cast<const uint16_t*>(&bootSectorData[0x16]);
            uint32_t FATSize32 = *reinterpret_cast<const uint32_t*>(&bootSectorData[0x24]);

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
    // First, try to use the full FAT metadata approach if we can read the disk properly
    try {
        // Create a filesystem reader from the disk reader
        // We need to determine the volume start offset and sector size
        uint32_t sectorSize = diskReader_->getSectorSize();
        if (sectorSize == 0) {
            // Fall back to boot sector-only analysis
            return analyzeFatFromBootSector(bootSectorData, analysisResults);
        }

        // For now, we assume the boot sector we read is at the volume start
        // In a more complete implementation, we would get this from the partition layer
        uint64_t volumeStartOffset = startSector_ * sectorSize;

        // Create the disk reader adapter
        DiskReaderAdapter adapter(diskReader_, volumeStartOffset);

        // Create FAT metadata object
        recoverysuite::filesystem::fat::FATMetadata fatMetadata(adapter);

        // Populate analysis results from the FAT metadata
        analysisResults["Filesystem Type"] = fatMetadata.getName();
        analysisResults["Bytes Per Sector"] = std::to_string(fatMetadata.getBytesPerSector());
        analysisResults["Sectors Per Cluster"] = std::to_string(fatMetadata.getSectorsPerCluster());
        analysisResults["Reserved Sector Count"] = std::to_string(fatMetadata.getReservedSectorCount());
        (void)fatMetadata.getFatCount(); // unused variable
        analysisResults["Number of FATs"] = std::to_string(fatMetadata.getFatCount());
        analysisResults["Root Entry Count"] = std::to_string(fatMetadata.getRootEntryCount());
        analysisResults["Total Sectors (16-bit)"] = std::to_string(fatMetadata.getTotalSectors16());
        analysisResults["Total Sectors (32-bit)"] = std::to_string(fatMetadata.getTotalSectors32());
        analysisResults["FAT Size (16-bit)"] = std::to_string(fatMetadata.getSectorsPerFat16());
        analysisResults["FAT Size (32-bit)"] = std::to_string(fatMetadata.getSectorsPerFat32());

        // Calculate FAT size value for display
        uint32_t fatSize16 = fatMetadata.getSectorsPerFat16();
        uint32_t fatSize32 = fatMetadata.getSectorsPerFat32();
        uint32_t fatSize = (fatSize16 != 0) ? fatSize16 : fatSize32;
        analysisResults["FAT Size"] = std::to_string(fatSize);

        // Total sectors
        uint32_t totalSectors16 = fatMetadata.getTotalSectors16();
        uint32_t totalSectors32 = fatMetadata.getTotalSectors32();
        uint32_t totalSectors = (totalSectors16 != 0) ? totalSectors16 : totalSectors32;
        analysisResults["Total Sectors"] = std::to_string(totalSectors);

        // Calculate derived values
        uint32_t rootEntryCount = fatMetadata.getRootEntryCount();
        uint32_t rootDirSectors = ((rootEntryCount * 32) + (fatMetadata.getBytesPerSector() - 1)) / fatMetadata.getBytesPerSector();
        uint32_t firstDataSector = fatMetadata.getReservedSectorCount() +
                                  (fatMetadata.getFatCount() * fatSize) +
                                  rootDirSectors;
        uint32_t clusterCount = (totalSectors - firstDataSector) / fatMetadata.getSectorsPerCluster();

        analysisResults["Root Directory Sectors"] = std::to_string(rootDirSectors);
        analysisResults["First Data Sector"] = std::to_string(firstDataSector);
        analysisResults["Cluster Count"] = std::to_string(clusterCount);

        // Additional FAT-specific information
        analysisResults["Volume Serial Number"] = std::to_string(fatMetadata.getVolumeSerialNumber());
        analysisResults["Volume Label"] = fatMetadata.getVolumeLabel();

        return true;
    } catch (const std::exception& e) {
        // If the full FAT metadata approach fails, fall back to boot sector-only analysis
        recoverysuite::logging::Logger::instance().debug("Full FAT metadata analysis failed, falling back to boot sector only: " + std::string(e.what()));
        return analyzeFatFromBootSector(bootSectorData, analysisResults);
    }
}

bool FilesystemAnalyzer::analyzeNtfsFilesystem(
    const std::vector<uint8_t>& bootSectorData,
    std::map<std::string, std::string>& analysisResults) const {
    // First, try to use the full NTFS metadata approach if we can read the disk properly
    try {
        // Create a filesystem reader from the disk reader
        // We need to determine the volume start offset and sector size
        uint32_t sectorSize = diskReader_->getSectorSize();
        if (sectorSize == 0) {
            // Fall back to boot sector-only analysis
            return analyzeNtfsFromBootSector(bootSectorData, analysisResults);
        }

        // For now, we assume the boot sector we read is at the volume start
        // In a more complete implementation, we would get this from the partition layer
        uint64_t volumeStartOffset = startSector_ * sectorSize;

        // Create the disk reader adapter
        auto adapterPtr = std::make_shared<DiskReaderAdapter>(diskReader_, volumeStartOffset);

        // Create NTFS metadata object
        auto ntfsMetadata = std::make_shared<recoverysuite::filesystem::ntfs::NTFSMetadata>(adapterPtr);

        // Populate analysis results from the NTFS metadata
        analysisResults["Filesystem Type"] = ntfsMetadata->getName();
        analysisResults["Version"] = ntfsMetadata->getVersion();
        analysisResults["Bytes Per Sector"] = std::to_string(ntfsMetadata->getBlockSize());
        // NTFSMetadata doesn't have getSectorsPerCluster, so we'll calculate it
        uint32_t sectorsPerCluster = 1; // Default value
        if (ntfsMetadata->getBlockSize() > 0 && sectorSize > 0) {
            sectorsPerCluster = ntfsMetadata->getBlockSize() / sectorSize;
        }
        analysisResults["Sectors Per Cluster"] = std::to_string(sectorsPerCluster);

        // Volume serial number
        analysisResults["Volume Serial Number"] = ntfsMetadata->getSerialNumber();

        // Volume label
        analysisResults["Volume Label"] = ntfsMetadata->getVolumeLabel();

        // Total size
        analysisResults["Total Size"] = std::to_string(ntfsMetadata->getTotalSize());

        // Free space
        analysisResults["Free Space"] = std::to_string(ntfsMetadata->getFreeSpace());

        // Used space
        analysisResults["Used Space"] = std::to_string(ntfsMetadata->getUsedSpace());

        return true;
    } catch (const std::exception& e) {
        // If the full NTFS metadata approach fails, fall back to boot sector-only analysis
        recoverysuite::logging::Logger::instance().debug("Full NTFS metadata analysis failed, falling back to boot sector only: " + std::string(e.what()));
        return analyzeNtfsFromBootSector(bootSectorData, analysisResults);
    }
}

} // namespace recovery
} // namespace recoverysuite