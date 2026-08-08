#ifndef RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_VOLUME_H
#define RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_VOLUME_H

#include <memory>
#include <string>
#include <cstdint>
#include "FilesystemVolume.h"
#include "NTFSConstants.h"
#include "NTFSContext.h"

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

// NTFS Volume implementation
class NTFSVolume : public FilesystemVolume {
public:
    explicit NTFSVolume(std::shared_ptr<FilesystemReader> reader);
    ~NTFSVolume() override = default;

    // FilesystemVolume interface
    std::string getVolumeIdentifier() const override;
    uint64_t getStartOffset() const override;
    uint64_t getSize() const override;
    uint64_t getEndOffset() const override;
    bool isBootable() const override;
    bool isActive() const override;
    bool isHidden() const override;
    uint8_t getPartitionType() const override;
    FilesystemType getFilesystemType() const override;
    bool isValid() const override;

private:
    std::shared_ptr<FilesystemReader> reader_;
    std::shared_ptr<NTFSContext> context_;
    
    // Internal volume methods
    bool readVolumeInfo() const;
};

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_VOLUME_H
