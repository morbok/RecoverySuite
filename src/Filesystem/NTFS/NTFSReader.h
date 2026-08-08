#ifndef RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_READER_H
#define RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_READER_H

#include <memory>
#include <cstdint>
#include "FilesystemReader.h"
#include "NTFSContext.h"

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

// NTFS Reader implementation
class NTFSReader : public FilesystemReader {
public:
    explicit NTFSReader(std::shared_ptr<FilesystemReader> underlyingReader);
    ~NTFSReader() override = default;

    // FilesystemReader interface
    uint32_t readSector(uint64_t sectorIndex, uint8_t* buffer, uint32_t sectorSize) override;
    uint32_t readSectors(uint64_t startSector, uint32_t sectorCount, uint8_t* buffer, uint32_t sectorSize) override;
    uint32_t readCluster(uint64_t clusterIndex, uint8_t* buffer, uint32_t clusterSize) override;
    uint32_t readClusters(uint64_t startCluster, uint32_t clusterCount, uint8_t* buffer, uint32_t clusterSize) override;
    uint64_t getTotalSectors() const override;
    uint32_t getSectorSize() const override;
    uint64_t getStartOffset() const override;
    void flush() override;

private:
    std::shared_ptr<FilesystemReader> underlyingReader_;
    std::shared_ptr<NTFSContext> context_;
};

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_READER_H
