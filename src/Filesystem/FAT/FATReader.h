#ifndef RECOVERYSUITE_FILESYSTEM_FAT_FATREADER_H
#define RECOVERYSUITE_FILESYSTEM_FAT_FATREADER_H

#include <cstdint>
#include <memory>
#include "../FilesystemReader.h"

namespace recoverysuite {
namespace filesystem {
namespace fat {

// FAT Reader provides read-only access to FAT filesystem data at the sector level.
class FATReader : public FilesystemReader {
public:
    // Constructor
    explicit FATReader(uint64_t startOffset, uint32_t sectorSize);

    // Destructor
    ~FATReader() override = default;

    // Read a sector from the disk
    // @param sectorIndex: the sector index to read (0-based, relative to the start of the FAT filesystem)
    // @param buffer: buffer to store the sector data (must be at least sectorSize bytes)
    // @param sectorSize: the size of a sector in bytes (usually 512 or 4096)
    // @return number of bytes read, or 0 on error
    uint32_t readSector(uint64_t sectorIndex, uint8_t* buffer, uint32_t sectorSize) override;

    // Read multiple consecutive sectors
    // @param startSector: the first sector to read (0-based, relative to the start of the FAT filesystem)
    // @param sectorCount: number of sectors to read
    // @param buffer: buffer to store the data (must be at least sectorCount * sectorSize bytes)
    // @param sectorSize: the size of a sector in bytes
    // @return number of bytes read, or 0 on error
    uint32_t readSectors(uint64_t startSector, uint32_t sectorCount, uint8_t* buffer, uint32_t sectorSize) override;

    // Read a cluster from the disk
    // @param clusterIndex: the cluster index to read (0-based)
    // @param buffer: buffer to store the cluster data (must be at least clusterSize bytes)
    // @param clusterSize: the size of a cluster in bytes
    // @return number of bytes read, or 0 on error
    uint32_t readCluster(uint64_t clusterIndex, uint8_t* buffer, uint32_t clusterSize) override;

    // Read multiple consecutive clusters
    // @param startCluster: the first cluster to read
    // @param clusterCount: number of clusters to read
    // @param buffer: buffer to store the data (must be at least clusterCount * clusterSize bytes)
    // @param clusterSize: the size of a cluster in bytes
    // @return number of bytes read, or 0 on error
    uint32_t readClusters(uint64_t startCluster, uint32_t clusterCount, uint8_t* buffer, uint32_t clusterSize) override;

    // Get the total number of sectors on the disk
    virtual uint64_t getTotalSectors() const override;

    // Get the sector size in bytes
    virtual uint32_t getSectorSize() const override;
    virtual uint64_t getStartOffset() const override;

    // Optional: flush any cached data (if implemented)
    virtual void flush() override {};

protected:
    // Start offset of the FAT filesystem (in bytes)
    uint64_t startOffset_;

    // Sector size in bytes
    uint32_t sectorSize_;
};

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FAT_FATREADER_H