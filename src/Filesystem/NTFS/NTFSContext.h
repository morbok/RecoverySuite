#ifndef RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_CONTEXT_H
#define RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_CONTEXT_H

#include <memory>
#include "FilesystemReader.h"
#include "NTFSConstants.h"

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

// NTFSContext holds contextual information needed by NTFS components during operation.
class NTFSContext {
public:
    explicit NTFSContext(std::shared_ptr<FilesystemReader> reader)
        : reader_(std::move(reader)) {}

    // Get the filesystem reader for accessing disk data
    std::shared_ptr<FilesystemReader> getReader() const { return reader_; }

    // Get the sector size from the reader
    uint32_t getSectorSize() const {
        return reader_ ? reader_->getSectorSize() : 512; // Default to 512 if no reader
    }

private:
    std::shared_ptr<FilesystemReader> reader_;
};

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_CONTEXT_H
