#ifndef RECOVERYSUITE_FILESYSTEM_FILESYSTEM_METADATA_H
#define RECOVERYSUITE_FILESYSTEM_FILESYSTEM_METADATA_H

#include <memory>
#include <string>
#include <cstdint>
#include "FilesystemExceptions.h"
#include "FilesystemTypes.h"

namespace recoverysuite {
namespace filesystem {

// FilesystemMetadata provides access to metadata about a filesystem.
class FilesystemMetadata {
public:
    virtual ~FilesystemMetadata() = default;

    // Get the filesystem type
    virtual FilesystemType getType() const = 0;

    // Get the filesystem name (e.g., "NTFS")
    virtual std::string getName() const = 0;

    // Get the version string (if applicable)
    virtual std::string getVersion() const = 0;

    // Get the serial number or unique identifier (if applicable)
    virtual std::string getSerialNumber() const = 0;

    // Get the volume label (if applicable)
    virtual std::string getVolumeLabel() const = 0;

    // Get the total size in bytes
    virtual uint64_t getTotalSize() const = 0;

    // Get the free space in bytes
    virtual uint64_t getFreeSpace() const = 0;

    // Get the used space in bytes
    virtual uint64_t getUsedSpace() const = 0;

    // Get the block size (sector size) in bytes
    virtual uint32_t getBlockSize() const = 0;

    // Get the number of blocks
    virtual uint64_t getBlockCount() const = 0;

    // Get the number of free blocks
    virtual uint64_t getFreeBlockCount() const = 0;

    // Check if the filesystem is read-only
    virtual bool isReadOnly() const = 0;

    // Check if the filesystem is mounted
    virtual bool isMounted() const = 0;
};

} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FILESYSTEM_METADATA_H
