#ifndef RECOVERYSUITE_FILESYSTEM_FILESYSTEM_READER_H
#define RECOVERYSUITE_FILESYSTEM_FILESYSTEM_READER_H

#include <cstdint>
#include <memory>
#include <vector>
#include "FilesystemExceptions.h"

namespace recoverysuite {
namespace filesystem {

// Forward declaration
class Filesystem;

// FilesystemReader provides read-only access to filesystem data at the sector/cluster level.
// It does not interpret filesystem structures - that is left to higher layers.
class FilesystemReader {
public:
    virtual ~FilesystemReader() = default;

    // Read a sector from the disk
    // @param sectorIndex: the sector index to read (0-based)
    // @param buffer: buffer to store the sector data (must be at least sectorSize bytes)
    // @param sectorSize: the size of a sector in bytes (usually 512 or 4096)
    // @return number of bytes read, or 0 on error
    virtual uint32_t readSector(uint64_t sectorIndex, uint8_t* buffer, uint32_t sectorSize) = 0;

    // Read multiple consecutive sectors
    // @param startSector: the first sector to read
    // @param sectorCount: number of sectors to read
    // @param buffer: buffer to store the data (must be at least sectorCount * sectorSize bytes)
    // @param sectorSize: the size of a sector in bytes
    // @return number of bytes read, or 0 on error
    virtual uint32_t readSectors(uint64_t startSector, uint32_t sectorCount, uint8_t* buffer, uint32_t sectorSize) = 0;

    // Read a cluster from the disk
    // @param clusterIndex: the cluster index to read (0-based)
    // @param buffer: buffer to store the cluster data (must be at least clusterSize bytes)
    // @param clusterSize: the size of a cluster in bytes
    // @return number of bytes read, or 0 on error
    virtual uint32_t readCluster(uint64_t clusterIndex, uint8_t* buffer, uint32_t clusterSize) = 0;

    // Read multiple consecutive clusters
    // @param startCluster: the first cluster to read
    // @param clusterCount: number of clusters to read
    // @param buffer: buffer to store the data (must be at least clusterCount * clusterSize bytes)
    // @param clusterSize: the size of a cluster in bytes
    // @return number of bytes read, or 0 on error
    virtual uint32_t readClusters(uint64_t startCluster, uint32_t clusterCount, uint8_t* buffer, uint32_t clusterSize) = 0;

    // Get the total number of sectors on the disk
    virtual uint64_t getTotalSectors() const = 0;

    // Get the sector size in bytes
    virtual uint32_t getSectorSize() const = 0;

    // Optional: flush any cached data (if implemented)
    virtual void flush() {}
};

} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FILESYSTEM_READER_H
