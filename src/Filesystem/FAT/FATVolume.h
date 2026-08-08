#ifndef RECOVERYSUITE_FILESYSTEM_FAT_FATVOLUME_H
#define RECOVERYSUITE_FILESYSTEM_FAT_FATVOLUME_H

#include <memory>
#include <string>
#include "../FilesystemVolume.h"
#include "../FilesystemReader.h"
#include "FATConstants.h"
#include "FATStructures.h"

namespace recoverysuite {
namespace filesystem {
namespace fat {

// FAT Volume represents a volume that contains a FAT filesystem.
class FATVolume : public FilesystemVolume {
public:
    // Constructor
    FATVolume(uint64_t startOffset, uint32_t sectorSize, const FATBootSectorCommon& bootSector);

    // Destructor
    ~FATVolume() override = default;

    // Get the volume identifier (e.g., disk number and partition offset)
    virtual std::string getVolumeIdentifier() const override;

    // Get the starting offset of the volume in bytes
    virtual uint64_t getStartOffset() const override;

    // Get the size of the volume in bytes
    virtual uint64_t getSize() const override;

    // Get the ending offset of the volume in bytes
    virtual uint64_t getEndOffset() const override;

    // Check if the volume is bootable
    virtual bool isBootable() const override;

    // Check if the volume is marked as active (for BIOS boot)
    virtual bool isActive() const override;

    // Check if the volume is hidden
    virtual bool isHidden() const override;

    // Get the partition type (if applicable)
    virtual uint8_t getPartitionType() const override;

    // Get the filesystem type contained in this volume
    virtual FilesystemType getFilesystemType() const override;

    // Check if the volume is valid (readable, etc.)
    virtual bool isValid() const override;

protected:
    // Start offset of the volume (in bytes)
    uint64_t startOffset_;

    // Sector size in bytes
    uint32_t sectorSize_;

    // FAT boot sector data
    FATBootSectorCommon bootSector_;

    // Calculate the total size of the volume in bytes
    uint64_t calculateSize() const;
};

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FAT_FATVOLUME_H