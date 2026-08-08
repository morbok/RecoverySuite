#include "FATVolume.h"
#include <sstream>
#include <iomanip>

namespace recoverysuite {
namespace filesystem {
namespace fat {

FATVolume::FATVolume(uint64_t startOffset, uint32_t sectorSize, const FATBootSectorCommon& bootSector)
    : startOffset_(startOffset), sectorSize_(sectorSize), bootSector_(bootSector) {
}

std::string FATVolume::getVolumeIdentifier() const {
    std::stringstream ss;
    ss << "Offset: 0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << startOffset_;
    return ss.str();
}

uint64_t FATVolume::getStartOffset() const {
    return startOffset_;
}

uint64_t FATVolume::getSize() const {
    return calculateSize();
}

uint64_t FATVolume::getEndOffset() const {
    if (startOffset_ == 0 && getSize() == 0) {
        return 0;
    }
    return startOffset_ + getSize() - 1;
}

bool FATVolume::isBootable() const {
    // A volume is bootable if it has a valid boot sector signature (already validated in metadata)
    return (bootSector_.bootSignature == FAT_BOOT_SIGNATURE);
}

// Check if the volume is marked as active (for BIOS boot)
bool FATVolume::isActive() const {
    // We don't have partition table information here, so return false as placeholder.
    return false;
}

// Check if the volume is hidden
bool FATVolume::isHidden() const {
    // We don't have partition table information here, so return false as placeholder.
    return false;
}

// Get the partition type (if applicable)
uint8_t FATVolume::getPartitionType() const {
    // We don't have partition table information here, so return 0 (unknown) as placeholder.
    return 0;
}

FilesystemType FATVolume::getFilesystemType() const {
    // Determine the FAT type from the boot sector
    // We'll use the same logic as in FATMetadata to determine the FAT type
    uint32_t totalSectors = (bootSector_.totalSectors16 != 0) ? bootSector_.totalSectors16 : bootSector_.totalSectors32;
    uint32_t reservedSectors = bootSector_.reservedSectorCount;
    uint32_t fatsSize = bootSector_.fatCount *
        ( (bootSector_.fatCount == 2 && bootSector_.sectorsPerFat16 != 0) ? bootSector_.sectorsPerFat16 : 0 ); // Simplified
    uint32_t rootDirSectors = ( (bootSector_.rootEntryCount * 32) + (bootSector_.bytesPerSector - 1) ) / bootSector_.bytesPerSector;
    uint32_t dataSectors = totalSectors - (reservedSectors + fatsSize + rootDirSectors);
    uint32_t clusterCount = dataSectors / bootSector_.sectorsPerCluster;

    if (clusterCount < fat::FAT12_MAX_CLUSTERS) {
        return FilesystemType::FAT12;
    } else if (clusterCount < fat::FAT16_MAX_CLUSTERS) {
        return FilesystemType::FAT16;
    } else {
        return FilesystemType::FAT32;
    }
}

bool FATVolume::isValid() const {
    // Check the boot sector signature
    if (bootSector_.bootSignature != FAT_BOOT_SIGNATURE) {
        return false;
    }

    // Check that bytes per sector is a valid value (usually 512, 1024, 2048, or 4096)
    uint16_t bps = bootSector_.bytesPerSector;
    if (bps != 512 && bps != 1024 && bps != 2048 && bps != 4096) {
        return false;
    }

    // Check that sectors per cluster is a power of 2 and <= 128
    uint8_t spc = bootSector_.sectorsPerCluster;
    if (spc == 0 || (spc & (spc - 1)) != 0 || spc > 128) {
        return false;
    }

    // Check that reserved sector count is at least 1 (for the boot sector)
    if (bootSector_.reservedSectorCount < 1) {
        return false;
    }

    // Check that fat count is reasonable (usually 1 or 2)
    if (bootSector_.fatCount == 0 || bootSector_.fatCount > 10) {
        return false;
    }

    // For FAT12/FAT16, check root entry count
    if (bootSector_.rootEntryCount == 0) {
        return false;
    }

    // Check that total sectors is reasonable (either 16 or 32 bit)
    uint32_t totalSectors16 = bootSector_.totalSectors16;
    uint32_t totalSectors32 = bootSector_.totalSectors32;
    if (totalSectors16 == 0 && totalSectors32 == 0) {
        return false;
    }

    return true;
}

uint64_t FATVolume::calculateSize() const {
    uint32_t totalSectors = (bootSector_.totalSectors16 != 0) ? bootSector_.totalSectors16 : bootSector_.totalSectors32;
    return static_cast<uint64_t>(totalSectors) * sectorSize_;
}

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite