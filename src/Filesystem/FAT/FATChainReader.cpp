#include "FATChainReader.h"
#include <stdexcept>
#include <algorithm>
#include <unordered_set>

namespace recoverysuite {
namespace filesystem {
namespace fat {

FATChainReader::FATChainReader(std::shared_ptr<FATTable> fatTable, uint32_t bytesPerSector, uint32_t sectorsPerCluster,
                               uint32_t reservedSectorCount, uint32_t fatCount, uint32_t sectorsPerFat)
    : fatTable_(fatTable),
      bytesPerSector_(bytesPerSector),
      sectorsPerCluster_(sectorsPerCluster),
      bytesPerCluster_(bytesPerSector * sectorsPerCluster),
      reservedSectorCount_(reservedSectorCount),
      fatCount_(fatCount),
      sectorsPerFat_(sectorsPerFat),
      dataRegionStartSector_(reservedSectorCount + fatCount * sectorsPerFat) {
    if (!fatTable_) {
        throw std::invalid_argument("FAT table cannot be null");
    }
    if (bytesPerSector_ == 0 || sectorsPerCluster_ == 0) {
        throw std::invalid_argument("Bytes per sector and sectors per cluster must be greater than zero");
    }
}

uint32_t FATChainReader::readChain(uint32_t startCluster, uint8_t* buffer, uint32_t bufferSize,
                                   uint32_t offset, uint32_t length) {
    if (!buffer || bufferSize == 0 || length == 0) {
        return 0;
    }

    // Adjust offset and length to be within the chain size
    uint64_t chainSize = getChainSize(startCluster);
    if (offset >= chainSize) {
        return 0; // Offset is beyond the end of the chain
    }
    if (offset + length > chainSize) {
        length = static_cast<uint32_t>(chainSize - offset); // Trim length to fit within chain
    }

    uint32_t bytesRead = 0;
    uint32_t currentOffset = 0; // Current offset within the chain

    // Get the cluster chain
    std::vector<uint32_t> chain = fatTable_->followClusterChain(startCluster);
    if (chain.empty()) {
        return 0;
    }

    // Iterate through each cluster in the chain
    for (uint32_t cluster : chain) {
        // Calculate the start offset of this cluster in the chain
        uint32_t clusterStartOffset = currentOffset;
        uint32_t clusterEndOffset = currentOffset + bytesPerCluster_;

        // Check if this cluster overlaps with the requested range
        if (clusterEndOffset <= offset) {
            // This cluster is entirely before the requested offset
            currentOffset = clusterEndOffset;
            continue;
        }
        if (clusterStartOffset >= offset + length) {
            // This cluster is entirely after the requested range
            break;
        }

        // Calculate the offset and length within this cluster
        uint32_t clusterOffset = 0;
        uint32_t clusterLength = 0;

        if (offset > clusterStartOffset) {
            // The requested offset is within this cluster
            clusterOffset = offset - clusterStartOffset;
        }
        // The available length in this cluster from the clusterOffset
        uint32_t availableInCluster = bytesPerCluster_ - clusterOffset;
        clusterLength = std::min(length - bytesRead, availableInCluster);

        // Read the data from this cluster
        uint32_t bytesReadFromCluster = readClusterSectors(cluster, buffer + bytesRead, 1, clusterOffset, clusterLength);
        bytesRead += bytesReadFromCluster;

        // If we've read the requested length, break
        if (bytesRead >= length) {
            break;
        }

        // Update current offset for the next cluster
        currentOffset = clusterEndOffset;
    }

    return bytesRead;
}

uint64_t FATChainReader::getChainSize(uint32_t startCluster) const {
    std::vector<uint32_t> chain = fatTable_->followClusterChain(startCluster);
    return static_cast<uint64_t>(chain.size()) * bytesPerCluster_;
}

bool FATChainReader::isValidChain(uint32_t startCluster) const {
    return fatTable_->isValidClusterChain(startCluster);
}

uint64_t FATChainReader::clusterToSector(uint32_t clusterNumber) const {
    // Cluster numbers 0 and 1 are reserved and do not have data sectors in the data region.
    // The data region starts at cluster 2.
    if (clusterNumber < 2) {
        // reserved clusters - they don't have data in the data region
        return 0;
    }

    // Calculate the sector offset for the given cluster
    // Each cluster occupies sectorsPerFat sectors
    uint64_t clusterOffset = static_cast<uint64_t>(clusterNumber - 2) * sectorsPerCluster_;
    return dataRegionStartSector_ + clusterOffset;
}

uint32_t FATChainReader::readClusterSectors(uint32_t clusterNumber, uint8_t* buffer, uint32_t sectorCount,
                                           uint32_t sectorOffset, uint32_t bytesToRead) {
    // We need to convert the cluster number to a sector offset on disk.
    (void)bytesToRead; // Suppress unused parameter warning
    uint64_t startSector = clusterToSector(clusterNumber);
    if (startSector == 0) {
        return 0; // Invalid cluster number (reserved cluster)
    }

    uint64_t targetSector = startSector + sectorOffset;
    if (targetSector < startSector) {
        // Overflow
        return 0;
    }

    // Read the sectors
    return fatTable_->readSectors(targetSector, sectorCount, buffer, bytesPerSector_);
}

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite