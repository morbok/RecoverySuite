#include "FATMetadata.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <limits>

namespace recoverysuite {
namespace filesystem {
namespace fat {

FATMetadata::FATMetadata(FilesystemReader& reader) {
    initialize(reader);
}

FilesystemType FATMetadata::getType() const {
    switch (fatType_) {
        case fat::FATType::FAT12: return FilesystemType::FAT12;
        case fat::FATType::FAT16: return FilesystemType::FAT16;
        case fat::FATType::FAT32: return FilesystemType::FAT32;
        default: return FilesystemType::UNKNOWN;
    }
}

std::string FATMetadata::getName() const {
    return fat::fatTypeToString(fatType_);
}

std::string FATMetadata::getVersion() const {
    // FAT filesystems don't have a version in the traditional sense
    return "";
}

std::string FATMetadata::getSerialNumber() const {
    // Return the volume serial number as a string
    uint32_t serial = getVolumeSerialNumber();
    char buffer[11];
    // Format as 8-digit hex with leading zeros
    snprintf(buffer, sizeof(buffer), "%08lX", static_cast<unsigned long>(serial));
    return std::string(buffer);
}

uint32_t FATMetadata::getVolumeSerialNumber() const {
    // Return the volume serial number from the BPB
    if (fatType_ == fat::FATType::FAT32) {
        return bootSector_.fat32.volumeID;
    } else {
        return bootSector_.fat16.volumeID;
    }
}

std::string FATMetadata::getVolumeLabel() const {
    // Volume label is 11 bytes, padded with spaces
    char label[12] = {0};
    if (fatType_ == fat::FATType::FAT32) {
        std::memcpy(label, bootSector_.fat32.volumeLabel, 11);
    } else {
        std::memcpy(label, bootSector_.fat16.volumeLabel, 11);
    }
    // Remove trailing spaces
    std::string str(label);
    str.erase(str.find_last_not_of(' ') + 1);
    return str;
}

fat::FATType FATMetadata::getFatType() const {
    return fatType_;
}

uint16_t FATMetadata::getBytesPerSector() const {
    return bootSector_.bytesPerSector;
}

uint8_t FATMetadata::getSectorsPerCluster() const {
    return bootSector_.sectorsPerCluster;
}

uint16_t FATMetadata::getReservedSectorCount() const {
    return bootSector_.reservedSectorCount;
}

uint8_t FATMetadata::getFatCount() const {
    return bootSector_.fatCount;
}

uint16_t FATMetadata::getRootEntryCount() const {
    return bootSector_.rootEntryCount;
}

uint16_t FATMetadata::getTotalSectors16() const {
    return bootSector_.totalSectors16;
}

uint32_t FATMetadata::getTotalSectors32() const {
    return bootSector_.totalSectors32;
}

uint16_t FATMetadata::getSectorsPerFat16() const {
    return bootSector_.sectorsPerFat16;
}

uint32_t FATMetadata::getSectorsPerFat32() const {
    if (fatType_ == fat::FATType::FAT32) {
        return bootSector_.fat32.sectorsPerFat32;
    }
    return 0;
}

uint64_t FATMetadata::getTotalSize() const {
    uint32_t totalSectors = (bootSector_.totalSectors16 != 0) ? bootSector_.totalSectors16 : bootSector_.totalSectors32;
    return static_cast<uint64_t>(totalSectors) * bootSector_.bytesPerSector;
}

uint64_t FATMetadata::getFreeSpace() const {
    // We don't have FAT table parsed, so we cannot compute free space accurately.
    // Return 0 as placeholder.
    return 0;
}

uint64_t FATMetadata::getUsedSpace() const {
    // We don't have FAT table parsed, so we cannot compute used space accurately.
    // Return 0 as placeholder.
    return 0;
}

uint32_t FATMetadata::getBlockSize() const {
    return bootSector_.bytesPerSector;
}

uint64_t FATMetadata::getBlockCount() const {
    uint32_t totalSectors = (bootSector_.totalSectors16 != 0) ? bootSector_.totalSectors16 : bootSector_.totalSectors32;
    return static_cast<uint64_t>(totalSectors);
}

uint64_t FATMetadata::getFreeBlockCount() const {
    // We don't have FAT table parsed, so we cannot compute free block count accurately.
    // Return 0 as placeholder.
    return 0;
}

bool FATMetadata::isReadOnly() const {
    // For now, we are implementing read-only foundation, so return true.
    return true;
}

bool FATMetadata::isMounted() const {
    // We don't have mounting concept in this layer, so return false.
    return false;
}

void FATMetadata::initialize(FilesystemReader& reader) {
    // Read the boot sector (sector 0 of the FAT filesystem)
    uint8_t buffer[512] = {0}; // Assume 512 bytes for now, but we should get the sector size from the reader
    // We need to know the sector size. Since we don't have it yet, we'll assume 512.
    // In a real implementation, we would get the sector size from the reader or from the disk parameters.
    // For now, we'll use 512 as a placeholder.
    uint32_t bytesRead = reader.readSector(0, buffer, 512);
    if (bytesRead < sizeof(FATBootSectorCommon)) {
        throw std::runtime_error("Failed to read FAT boot sector");
    }

    // Copy the boot sector data
    std::memcpy(&bootSector_, buffer, sizeof(FATBootSectorCommon));

    // Validate the boot sector signature
    if (bootSector_.bootSignature != FAT_BOOT_SIGNATURE) {
        throw std::runtime_error("Invalid FAT boot sector signature");
    }

    // Determine the FAT type based on the cluster count
    // Calculate the number of clusters
    uint32_t totalSectors = (bootSector_.totalSectors16 != 0) ? bootSector_.totalSectors16 : bootSector_.totalSectors32;
    uint32_t reservedSectors = bootSector_.reservedSectorCount;
    uint32_t fatsSize = bootSector_.fatCount *
        ( (bootSector_.fatCount == 2 && bootSector_.sectorsPerFat16 != 0) ? bootSector_.sectorsPerFat16 : 0 );
    uint32_t rootDirSectors = ( (bootSector_.rootEntryCount * 32) + (bootSector_.bytesPerSector - 1) ) / bootSector_.bytesPerSector;
    uint32_t dataSectors = totalSectors - (reservedSectors + fatsSize + rootDirSectors);
    uint32_t clusterCount = dataSectors / bootSector_.sectorsPerCluster;

    fatType_ = calculateFatType(clusterCount);
}

FATType FATMetadata::calculateFatType(uint32_t clusterCount) const {
    if (clusterCount < fat::FAT12_MAX_CLUSTERS) {
        return fat::FATType::FAT12;
    } else if (clusterCount < fat::FAT16_MAX_CLUSTERS) {
        return fat::FATType::FAT16;
    } else {
        return fat::FATType::FAT32;
    }
}

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite