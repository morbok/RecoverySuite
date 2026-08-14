#include "NTFSMetadata.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include "NTFSStructures.h"

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

NTFSMetadata::NTFSMetadata(std::shared_ptr<FilesystemReader> reader)
    : reader_(std::move(reader)), 
      context_(std::make_shared<NTFSContext>(reader_)) {}


FilesystemType NTFSMetadata::getType() const {
    return FilesystemType::NTFS;
}

std::string NTFSMetadata::getName() const {
    return "NTFS";
}

std::string NTFSMetadata::getVersion() const {
    // In a real implementation, we would read the version from the boot sector
    // For now, return a placeholder
    if (!reader_) {
        return "Unknown";
    }
    
    // For NTFS, version info is in the boot sector
    const uint32_t sectorSize = reader_->getSectorSize();
    if (sectorSize == 0) {
        return "Unknown";
    }
    
    std::vector<uint8_t> buffer(sectorSize);
    uint32_t bytesRead = reader_->readSector(0, buffer.data(), sectorSize);
    
    if (bytesRead != sectorSize) {
        return "Unknown";
    }

    const BootSector* bootSector = reinterpret_cast<const BootSector*>(buffer.data());
    // NTFS version is in the boot sector (bytes 0x4A-0x4B for major/minor)
    // For simplicity, we'll return a fixed version
    (void)bootSector; // Suppress unused variable warning
    return "3.1"; // Most common NTFS version
}

std::string NTFSMetadata::getSerialNumber() const {
    // In a real implementation, we would read the volume serial number from the boot sector
    if (!reader_) {
        return "";
    }
    
    const uint32_t sectorSize = reader_->getSectorSize();
    if (sectorSize == 0) {
        return "";
    }
    
    std::vector<uint8_t> buffer(sectorSize);
    uint32_t bytesRead = reader_->readSector(0, buffer.data(), sectorSize);
    
    if (bytesRead != sectorSize) {
        return "";
    }
    
    const BootSector* bootSector = reinterpret_cast<const BootSector*>(buffer.data());
    // Volume serial number is at offset 0x36 (4 bytes)
    uint32_t serialNumber = 0;
    memcpy(&serialNumber, bootSector->volumeSerialNumber, sizeof(serialNumber));
    
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') 
       << std::setw(8) << serialNumber;
    return ss.str();
}

std::string NTFSMetadata::getVolumeLabel() const {
    // In a real implementation, we would read the volume label from the $Volume attribute
    // For now, return empty as placeholder
    return "";
}

uint64_t NTFSMetadata::getTotalSize() const {
    // In a real implementation, we would calculate total size from boot sector info
    if (!reader_) {
        return 0;
    }
    
    const uint32_t sectorSize = reader_->getSectorSize();
    if (sectorSize == 0) {
        return 0;
    }
    
    std::vector<uint8_t> buffer(sectorSize);
    uint32_t bytesRead = reader_->readSector(0, buffer.data(), sectorSize);
    
    if (bytesRead != sectorSize) {
        return 0;
    }
    
    const BootSector* bootSector = reinterpret_cast<const BootSector*>(buffer.data());
    
    // Use the 64-bit sector count if available, otherwise use 32-bit
    uint64_t totalSectors = bootSector->totalSectorCount64;
    if (totalSectors == 0) {
        totalSectors = bootSector->totalSectorCount32;
    }
    
    return totalSectors * sectorSize;
}

uint64_t NTFSMetadata::getFreeSpace() const {
    // In a real implementation, we would read the bitmap or other metadata to determine free space
    // For now, return 0 as placeholder
    return 0;
}

uint64_t NTFSMetadata::getUsedSpace() const {
    // In a real implementation, we would calculate used space
    // For now, return 0 as placeholder
    return 0;
}

uint32_t NTFSMetadata::getBlockSize() const {
    // For NTFS, block size is typically the same as sector size, but can be larger based on clusters
    if (!reader_) {
        return 512; // Default
    }
    
    return reader_->getSectorSize();
}

uint64_t NTFSMetadata::getBlockCount() const {
    // In a real implementation, we would get this from boot sector info
    if (!reader_) {
        return 0;
    }
    
    const uint32_t sectorSize = reader_->getSectorSize();
    if (sectorSize == 0) {
        return 0;
    }
    
    uint64_t totalSize = getTotalSize();
    if (totalSize == 0) {
        return 0;
    }
    
    return totalSize / sectorSize;
}

uint64_t NTFSMetadata::getFreeBlockCount() const {
    // In a real implementation, we would calculate this from free space and block size
    if (!reader_) {
        return 0;
    }
    
    uint64_t freeSpace = getFreeSpace();
    uint32_t blockSize = getBlockSize();
    
    if (blockSize == 0) {
        return 0;
    }
    
    return freeSpace / blockSize;
}

bool NTFSMetadata::isReadOnly() const {
    // In a real implementation, we would check if the volume is mounted as read-only
    // For now, return false as placeholder (assuming read-write)
    return false;
}

bool NTFSMetadata::isMounted() const {
    // In a real implementation, we would check if the volume is currently mounted
    // For our read-only forensic tool, we typically work with unmounted volumes
    return false;
}

bool NTFSMetadata::readBootSectorInfo() const {
    if (!reader_) {
        return false;
    }
    
    const uint32_t sectorSize = reader_->getSectorSize();
    if (sectorSize == 0) {
        return false;
    }
    
    std::vector<uint8_t> buffer(sectorSize);
    uint32_t bytesRead = reader_->readSector(0, buffer.data(), sectorSize);
    
    return bytesRead == sectorSize;
}

bool NTFSMetadata::readVolumeInfo() const {
    // In a real implementation, we would read more detailed volume information
    // For now, just read the boot sector
    return readBootSectorInfo();
}

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite
