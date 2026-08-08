#ifndef RECOVERYSUITE_FILESYSTEM_FILESYSTEM_VOLUME_H
#define RECOVERYSUITE_FILESYSTEM_FILESYSTEM_VOLUME_H

#include <memory>
#include <string>
#include <cstdint>
#include "FilesystemExceptions.h"
#include "FilesystemTypes.h"

namespace recoverysuite {
namespace filesystem {

// FilesystemVolume represents a volume that contains a filesystem.
class FilesystemVolume {
public:
    virtual ~FilesystemVolume() = default;

    // Get the volume identifier (e.g., disk number and partition offset)
    virtual std::string getVolumeIdentifier() const = 0;

    // Get the starting offset of the volume in bytes
    virtual uint64_t getStartOffset() const = 0;

    // Get the size of the volume in bytes
    virtual uint64_t getSize() const = 0;

    // Get the ending offset of the volume in bytes
    virtual uint64_t getEndOffset() const = 0;

    // Check if the volume is bootable
    virtual bool isBootable() const = 0;

    // Check if the volume is marked as active (for BIOS boot)
    virtual bool isActive() const = 0;

    // Check if the volume is hidden
    virtual bool isHidden() const = 0;

    // Get the partition type (if applicable)
    virtual uint8_t getPartitionType() const = 0;

    // Get the filesystem type contained in this volume
    virtual FilesystemType getFilesystemType() const = 0;

    // Check if the volume is valid (readable, etc.)
    virtual bool isValid() const = 0;
};

} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FILESYSTEM_VOLUME_H
