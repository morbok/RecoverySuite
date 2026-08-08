#include "NTFSFilesystem.h"
#include "NTFSReader.h"
#include "NTFSAnalyzer.h"
#include "NTFSMetadata.h"
#include "NTFSVolume.h"

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

NTFSFilesystem::NTFSFilesystem(std::shared_ptr<FilesystemReader> reader)
    : reader_(std::move(reader)), 
      context_(std::make_shared<NTFSContext>(reader_)) {}

FilesystemType NTFSFilesystem::getType() const {
    return FilesystemType::NTFS;
}

std::string NTFSFilesystem::getName() const {
    return "NTFS";
}

std::string NTFSFilesystem::getVersion() const {
    // In a real implementation, we would read the version from the boot sector
    return "1.0"; // Placeholder
}

bool NTFSFilesystem::supportsCapability(FilesystemCapability capability) const {
    // NTFS supports read, write, recovery, metadata, journaling, compression, encryption, sparse files, etc.
    switch (capability) {
        case FilesystemCapability::READ_SUPPORT:
        case FilesystemCapability::WRITE_SUPPORT:
        case FilesystemCapability::RECOVERY_SUPPORT:
        case FilesystemCapability::METADATA_SUPPORT:
        case FilesystemCapability::JOURNAL_SUPPORT:
        case FilesystemCapability::COMPRESSION_SUPPORT:
        case FilesystemCapability::ENCRYPTION_SUPPORT:
        case FilesystemCapability::SPARSE_FILE_SUPPORT:
            return true;
        default:
            return false;
    }
}

FilesystemCapabilities NTFSFilesystem::getCapabilities() const {
    // Return all capabilities that NTFS supports
    return static_cast<uint64_t>(
        FilesystemCapability::READ_SUPPORT |
        FilesystemCapability::WRITE_SUPPORT |
        FilesystemCapability::RECOVERY_SUPPORT |
        FilesystemCapability::METADATA_SUPPORT |
        FilesystemCapability::JOURNAL_SUPPORT |
        FilesystemCapability::COMPRESSION_SUPPORT |
        FilesystemCapability::ENCRYPTION_SUPPORT |
        FilesystemCapability::SPARSE_FILE_SUPPORT |
        FilesystemCapability::EXTENDED_ATTRIBUTES_SUPPORT
    );
}

std::unique_ptr<FilesystemReader> NTFSFilesystem::createReader() const {
    if (!reader_) {
        return nullptr;
    }
    return std::make_unique<NTFSReader>(reader_);
}

std::unique_ptr<FilesystemAnalyzer> NTFSFilesystem::createAnalyzer() const {
    if (!reader_) {
        return nullptr;
    }
    return std::make_unique<NTFSAnalyzer>(reader_);
}

std::unique_ptr<FilesystemMetadata> NTFSFilesystem::getMetadata() const {
    if (!reader_) {
        return nullptr;
    }
    return std::make_unique<NTFSMetadata>(reader_);
}

std::unique_ptr<FilesystemVolume> NTFSFilesystem::getVolume() const {
    if (!reader_) {
        return nullptr;
    }
    return std::make_unique<NTFSVolume>(reader_);
}

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite
