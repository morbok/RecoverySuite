#ifndef RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_METADATA_H
#define RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_METADATA_H

#include <memory>
#include <string>
#include <cstdint>
#include "FilesystemMetadata.h"
#include "NTFSConstants.h"
#include "NTFSContext.h"

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

// NTFS Metadata implementation
class NTFSMetadata : public FilesystemMetadata {
public:
    explicit NTFSMetadata(std::shared_ptr<FilesystemReader> reader);
    ~NTFSMetadata() override = default;

    // FilesystemMetadata interface
    FilesystemType getType() const override;
    std::string getName() const override;
    std::string getVersion() const override;
    std::string getSerialNumber() const override;
    std::string getVolumeLabel() const override;
    uint64_t getTotalSize() const override;
    uint64_t getFreeSpace() const override;
    uint64_t getUsedSpace() const override;
    uint32_t getBlockSize() const override;
    uint64_t getBlockCount() const override;
    uint64_t getFreeBlockCount() const override;
    bool isReadOnly() const override;
    bool isMounted() const override;

private:
    std::shared_ptr<FilesystemReader> reader_;
    std::shared_ptr<NTFSContext> context_;
    
    // Internal metadata methods
    bool readBootSectorInfo() const;
    bool readVolumeInfo() const;
};

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_METADATA_H
