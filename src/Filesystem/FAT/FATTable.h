#ifndef RECOVERYSUITE_FILESYSTEM_FAT_FATTABLE_H
#define RECOVERYSUITE_FILESYSTEM_FAT_FATTABLE_H

#include <cstdint>
#include <vector>
#include <memory>
#include <stdexcept>
#include "FATConstants.h"
#include "FATEntry.h"
#include "FATClusterState.h"
#include "../FilesystemReader.h"

namespace recoverysuite {
namespace filesystem {
namespace fat {

// Forward declaration
class FATTableParser;

/**
 * FAT Table representation
 * Provides read-only access to FAT entries and cluster chain information
 */
class FATTable {
public:
    // Constructor
    FATTable(FATType fatType, uint32_t sectorsPerFat, uint32_t bytesPerSector,
             uint32_t clusterCount, uint32_t reservedSectorCount, std::shared_ptr<FilesystemReader> reader);

    // Destructor
    ~FATTable() = default;

    // Delete copy constructor and assignment to prevent accidental copying
    FATTable(const FATTable&) = delete;
    FATTable& operator=(const FATTable&) = delete;

    // Get FAT type
    FATType getFatType() const { return fatType_; }

    // Get table parameters
    uint32_t getSectorsPerFat() const { return sectorsPerFat_; }
    uint32_t getBytesPerSector() const { return bytesPerSector_; }
    uint32_t getClusterCount() const { return clusterCount_; }
    uint32_t getFirstDataCluster() const {
        // First data cluster is always 2 for FAT12/16/32
        return 2;
    }

    // Get FAT entry for a specific cluster
    FATEntry getEntry(uint32_t clusterIndex) const;

    // Get cluster state information
    FATClusterInfo getClusterState(uint32_t clusterIndex) const;

    // Check if a cluster is bad
    bool isBadCluster(uint32_t clusterIndex) const;

    // Validate the FAT table structure
    bool validate() const;

    // Validate a single FAT entry
    bool isValidEntry(const FATEntry& entry, uint32_t clusterIndex) const;

    // Get statistics about the FAT table
    struct Statistics {
        uint32_t freeClusters = 0;
        uint32_t allocatedClusters = 0;
        uint32_t badClusters = 0;
        uint32_t reservedClusters = 0;
        uint32_t endOfChainClusters = 0;
        uint32_t totalClusters = 0;
    };

    Statistics getStatistics() const;

    // Follow a cluster chain starting from the given cluster
    // Returns vector of cluster numbers in the chain
    std::vector<uint32_t> followClusterChain(uint32_t startCluster) const;

    // Check if a cluster chain is valid (no cycles, valid ranges, etc.)
    bool isValidClusterChain(uint32_t startCluster) const;

    // Read sectors from the underlying storage
    uint32_t readSectors(uint64_t sectorOffset, uint32_t sectorCount, uint8_t* buffer, uint32_t bytesPerSector) const;

private:
    // FAT type (FAT12, FAT16, FAT32)
    FATType fatType_;

    // Reserved sector count (including boot sector)
    uint32_t reservedSectorCount_;

    // Table parameters
    uint32_t sectorsPerFat_;
    uint32_t bytesPerSector_;
    uint32_t clusterCount_;  // Maximum cluster number + 1

    // Reader for accessing the FAT data on disk
    std::shared_ptr<FilesystemReader> reader_;

    // Cached FAT data (loaded on demand)
    mutable std::vector<uint8_t> fatData_;
    mutable bool fatDataLoaded_ = false;

    // Load FAT data from disk
    void loadFatData() const;

    // Calculate the sector offset for a given FAT sector index
    uint64_t getFatSectorOffset(uint32_t fatSectorIndex) const;
};

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FAT_FATTABLE_H