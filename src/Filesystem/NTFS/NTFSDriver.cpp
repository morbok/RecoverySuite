#include "NTFSDriver.h"
#include "NTFSFilesystem.h"
#include "NTFSConstants.h"
#include <cstring>
#include "ntfs_boot_fields.h"

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
    return parseNtfsBootFields(bootSectorData,bootSectorSize).has_value();
}

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite
