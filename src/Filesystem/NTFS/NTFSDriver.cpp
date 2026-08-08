#include "NTFSDriver.h"
#include "NTFSFilesystem.h"
#include "NTFSConstants.h"
#include <cstring>

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

FilesystemType NTFSDriver::getFilesystemType() const {
    return FilesystemType::NTFS;
}

std::string NTFSDriver::getName() const {
    return "NTFS Driver";
}

double NTFSDriver::probe(const uint8_t* bootSectorData, uint32_t bootSectorSize) const {
    if (!bootSectorData || bootSectorSize < sizeof(BootSector)) {
        return 0.0;
    }
    
    // Validate NTFS boot sector
    if (validateBootSector(bootSectorData, bootSectorSize)) {
        return 1.0; // Confident match
    }
    
    return 0.0;
}

std::shared_ptr<Filesystem> NTFSDriver::createFilesystem() const {
    // Note: In a real implementation, we would pass the actual reader here
    // For now, we return a null pointer to indicate the framework is in place
    // The actual reader would be provided by the filesystem layer when creating the filesystem
    return nullptr;
}

std::vector<FilesystemType> NTFSDriver::getSupportedTypes() const {
    return {FilesystemType::NTFS};
}

bool NTFSDriver::validateBootSector(const uint8_t* bootSectorData, uint32_t bootSectorSize) const {
    if (!bootSectorData || bootSectorSize < sizeof(BootSector)) {
        return false;
    }
    
    const BootSector* bootSector = reinterpret_cast<const BootSector*>(bootSectorData);
    
    // Check NTFS signature
    if (bootSector->oemId[0] != 'N' || bootSector->oemId[1] != 'T' ||
        bootSector->oemId[2] != 'F' || bootSector->oemId[3] != 'S') {
        return false;
    }
    
    // Check bytes per sector (should be power of 2 and reasonable)
    uint16_t bytesPerSector = bootSector->bytesPerSector;
    if (bytesPerSector < 256 || bytesPerSector > 4096 || (bytesPerSector & (bytesPerSector - 1)) != 0) {
        return false;
    }
    
    // Check sectors per cluster (should be power of 2 and reasonable)
    uint8_t sectorsPerCluster = bootSector->sectorsPerCluster;
    if (sectorsPerCluster == 0 || sectorsPerCluster > 128 || (sectorsPerCluster & (sectorsPerCluster - 1)) != 0) {
        return false;
    }
    
    // Check boot signature
    if (bootSector->bootSignature != 0xAA55) {
        return false;
    }
    
    return true;
}

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite
