#ifndef RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_FILESYSTEM_H
#define RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_FILESYSTEM_H

#include <memory>
#include "Filesystem.h"
#include "NTFSConstants.h"
#include "NTFSContext.h"

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

// Forward declarations
class NTFSReader;
class NTFSAnalyzer;
class NTFSMetadata;
class NTFSVolume;

// NTFS Filesystem implementation
class NTFSFilesystem : public Filesystem {
public:
    explicit NTFSFilesystem(std::shared_ptr<FilesystemReader> reader);
    ~NTFSFilesystem() override = default;

    // Filesystem interface
    FilesystemType getType() const override;
    std::string getName() const override;
    std::string getVersion() const override;
    bool supportsCapability(FilesystemCapability capability) const override;
    FilesystemCapabilities getCapabilities() const override;
    std::unique_ptr<FilesystemReader> createReader() const override;
    std::unique_ptr<FilesystemAnalyzer> createAnalyzer() const override;
    std::unique_ptr<FilesystemMetadata> getMetadata() const override;
    std::unique_ptr<FilesystemVolume> getVolume() const override;

private:
    std::shared_ptr<FilesystemReader> reader_;
    std::shared_ptr<NTFSContext> context_;
};

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_FILESYSTEM_H
