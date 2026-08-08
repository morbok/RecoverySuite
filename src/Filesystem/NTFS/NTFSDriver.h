#ifndef RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_DRIVER_H
#define RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_DRIVER_H

#include <memory>
#include <string>
#include <vector>
#include "FilesystemDriver.h"
#include "NTFSConstants.h"
#include "NTFSStructures.h"

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

// Forward declaration
class NTFSFilesystem;

// NTFS Driver implementation
class NTFSDriver : public FilesystemDriver {
public:
    // FilesystemDriver interface
    FilesystemType getFilesystemType() const override;
    std::string getName() const override;
    double probe(const uint8_t* bootSectorData, uint32_t bootSectorSize) const override;
    std::shared_ptr<Filesystem> createFilesystem() const override;
    std::vector<FilesystemType> getSupportedTypes() const override;

private:
    // Helper to validate NTFS boot sector
    bool validateBootSector(const uint8_t* bootSectorData, uint32_t bootSectorSize) const;
};

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_DRIVER_H
