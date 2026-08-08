#include "NTFSVolume.h"
#include <stdexcept>
#include <sstream>
#include <vector>
#include "NTFSStructures.h"

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

NTFSVolume::NTFSVolume(std::shared_ptr<FilesystemReader> reader)
    : reader_(std::move(reader)), 
      context_(std::make_shared<NTFSContext>(reader_)) {}


std::string NTFSVolume::getVolumeIdentifier() const {
    // In a real implementation, we would return disk number and partition offset
    // For now, return a placeholder
    if (!reader_) {
        return "Unknown Volume";
    }
    
    return "Volume (disk info not available in this context)";
}

uint64_t NTFSVolume::getStartOffset() const {
    // In a real implementation, we would get this from partition information
    // For now, return 0 as placeholder (assuming volume starts at beginning of disk)
    return 0;
}

uint64_t NTFSVolume::getSize() const {
    // Get the size from metadata
    if (!reader_) {
        return 0;
    }
    
    // We could store a reference to metadata, but for now just calculate it
    // In a full implementation, this would be more efficient
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

uint64_t NTFSVolume::getEndOffset() const {
    uint64_t startOffset = getStartOffset();
    uint64_t size = getSize();
    
    if (startOffset == 0 && size == 0) {
        return 0;
    }
    
    return startOffset + size;
}

bool NTFSVolume::isBootable() const {
    // In a real implementation, we would check the active partition flag
    // For now, return false as placeholder
    return false;
}

bool NTFSVolume::isActive() const {
    // In a real implementation, we would check if this is the active/boot partition
    // For now, return false as placeholder
    return false;
}

bool NTFSVolume::isHidden() const {
    // In a real implementation, we would check partition attributes
    // For now, return false as placeholder
    return false;
}

uint8_t NTFSVolume::getPartitionType() const {
    // In a real implementation, we would get this from the partition table
    // For now, return 0 as placeholder (unknown)
    return 0;
}

FilesystemType NTFSVolume::getFilesystemType() const {
    return FilesystemType::NTFS;
}

bool NTFSVolume::isValid() const {
    // Check if we can read the boot sector and it's valid
    if (!reader_) {
        return false;
    }
    
    const uint32_t sectorSize = reader_->getSectorSize();
    if (sectorSize == 0) {
        return false;
    }
    
    std::vector<uint8_t> buffer(sectorSize);
    uint32_t bytesRead = reader_->readSector(0, buffer.data(), sectorSize);
    
    if (bytesRead != sectorSize) {
        return false;
    }
    
    // Validate the boot sector
    const BootSector* bootSector = reinterpret_cast<const BootSector*>(buffer.data());
    
    // Check NTFS signature
    if (bootSector->oemId[0] != 'N' || bootSector->oemId[1] != 'T' ||
        bootSector->oemId[2] != 'F' || bootSector->oemId[3] != 'S') {
        return false;
    }
    
    // Check boot signature
    if (bootSector->bootSignature != 0xAA55) {
        return false;
    }
    
    return true;
}

bool NTFSVolume::readVolumeInfo() const {
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

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite
