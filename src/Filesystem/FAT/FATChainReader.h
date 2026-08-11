#ifndef RECOVERYSUITE_FILESYSTEM_FAT_FATCHAINREADER_H
#define RECOVERYSUITE_FILESYSTEM_FAT_FATCHAINREADER_H

#include <cstdint>
#include <vector>
#include <memory>
#include "FATTable.h"
#include "../FilesystemReader.h"

namespace recoverysuite {
namespace filesystem {
namespace fat {

/**
 * FAT Chain Reader
 * Provides functionality to read file data by following cluster chains
 */
class FATChainReader {
public:
    // Constructor
    FATChainReader(std::shared_ptr<FATTable> fatTable, uint32_t bytesPerSector, uint32_t sectorsPerCluster,
                   uint32_t reservedSectorCount, uint32_t fatCount, uint32_t sectorsPerFat);

    // Destructor
    ~FATChainReader() = default;

    // Read data from a cluster chain into a buffer
    // @param startCluster: the first cluster of the chain
    // @param buffer: buffer to store the data
    // @param bufferSize: size of the buffer in bytes
    // @param offset: offset within the cluster chain to start reading from (in bytes)
    // @param length: number of bytes to read
    // @return number of bytes read, or 0 on error
    uint32_t readChain(uint32_t startCluster, uint8_t* buffer, uint32_t bufferSize,
                       uint32_t offset, uint32_t length);

    // Get the size of a cluster chain in bytes
    uint64_t getChainSize(uint32_t startCluster) const;

    // Check if a cluster chain is valid (no cycles, valid clusters, etc.)
    bool isValidChain(uint32_t startCluster) const;

private:
    // FAT table for accessing cluster entries
    std::shared_ptr<FATTable> fatTable_;

    // Disk parameters
    uint32_t bytesPerSector_;
    uint32_t sectorsPerCluster_;
    uint32_t bytesPerCluster_;  // bytesPerSector_ * sectorsPerCluster_

    // FAT layout parameters
    uint32_t reservedSectorCount_;
    uint32_t fatCount_;
    uint32_t sectorsPerFat_;
    uint32_t dataRegionStartSector_;  // First sector of cluster 2

    // Helper to convert cluster number to sector offset
    uint64_t clusterToSector(uint32_t clusterNumber) const;

    // Helper to read sectors from a cluster
    uint32_t readClusterSectors(uint32_t clusterNumber, uint8_t* buffer, uint32_t sectorCount,
                               uint32_t sectorOffset, uint32_t bytesToRead);
};

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FAT_FATCHAINREADER_H