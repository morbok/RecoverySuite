#include "FATTable.h"
#include "FATEntry.h"
#include <stdexcept>
#include <algorithm>
#include <unordered_set>
#include <iostream>
#include <limits>

namespace recoverysuite {
namespace filesystem {
namespace fat {

FATTable::FATTable(FATType fatType, uint32_t sectorsPerFat, uint32_t bytesPerSector,
                   uint32_t clusterCount, uint32_t reservedSectorCount, std::shared_ptr<FilesystemReader> reader)
    : fatType_(fatType),
      reservedSectorCount_(reservedSectorCount),
      sectorsPerFat_(sectorsPerFat),
      bytesPerSector_(bytesPerSector),
      clusterCount_(clusterCount),
      reader_(reader) {
    if (!reader_) {
        throw std::invalid_argument("Reader cannot be null");
    }
    if (sectorsPerFat_ == 0 || bytesPerSector_ == 0) {
        throw std::invalid_argument("Sectors per FAT and bytes per sector must be greater than zero");
    }
    if (clusterCount_ < 2) {
        throw std::invalid_argument("Cluster count must be at least 2");
    }
}

FATEntry FATTable::getEntry(uint32_t clusterIndex) const {
    if (!isValidClusterNumber(clusterIndex, clusterCount_)) {
        // Return an invalid entry for out-of-range cluster indices
        return FATEntry(FATType::UNKNOWN, 0);
    }

    loadFatData();

    // Calculate which sector in the FAT contains this entry
    switch (fatType_) {
        case FATType::FAT12: {
            uint16_t value = FAT12Entry::getEntry(fatData_.data(), clusterIndex, sectorsPerFat_, bytesPerSector_);
            return FATEntry(FATType::FAT12, value);
        }
        case FATType::FAT16: {
            uint16_t value = FAT16Entry::getEntry(fatData_.data(), clusterIndex, sectorsPerFat_, bytesPerSector_);
            return FATEntry(FATType::FAT16, value);
        }
        case FATType::FAT32: {
            uint32_t value = FAT32Entry::getEntry(fatData_.data(), clusterIndex, sectorsPerFat_, bytesPerSector_);
            return FATEntry(FATType::FAT32, value);
        }
        default:
            return FATEntry(FATType::UNKNOWN, 0);
    }
}

FATClusterInfo FATTable::getClusterState(uint32_t clusterIndex) const {
    if (!isValidClusterNumber(clusterIndex, clusterCount_)) {
        return FATClusterInfo(clusterIndex, ClusterState::UNKNOWN, 0, false);
    }

    FATEntry entry = getEntry(clusterIndex);

    if (!isValidEntry(entry, clusterIndex)) {
        return FATClusterInfo(clusterIndex, ClusterState::UNKNOWN, 0, false);
    }

    ClusterState state = ClusterState::UNKNOWN;
    uint32_t nextCluster = 0;

    switch (entry.getType()) {
        case FATType::FAT12: {
            uint16_t value = static_cast<uint16_t>(entry.getValue());
            if (FAT12Entry::isFree(value)) {
                state = ClusterState::FREE;
            } else if (FAT12Entry::isBad(value)) {
                state = ClusterState::BAD;
            } else if (value >= 0xFF0 && value <= 0xFF6) {  // Reserved in FAT12
                state = ClusterState::RESERVED;
            } else if (FAT12Entry::isEndOfChain(value)) {
                state = ClusterState::END_OF_CHAIN;
            } else {
                state = ClusterState::ALLOCATED;
                nextCluster = value;
            }
            break;
        }
        case FATType::FAT16: {
            uint16_t value = static_cast<uint16_t>(entry.getValue());
            if (FAT16Entry::isFree(value)) {
                state = ClusterState::FREE;
            } else if (FAT16Entry::isBad(value)) {
                state = ClusterState::BAD;
            } else if (value >= 0xFFF0 && value <= 0xFFFE) {  // Reserved in FAT16
                state = ClusterState::RESERVED;
            } else if (FAT16Entry::isEndOfChain(value)) {
                state = ClusterState::END_OF_CHAIN;
            } else {
                state = ClusterState::ALLOCATED;
                nextCluster = value;
            }
            break;
        }
        case FATType::FAT32: {
            uint32_t value = entry.getValue();
            if (FAT32Entry::isFree(value)) {
                state = ClusterState::FREE;
            } else if (FAT32Entry::isBad(value)) {
                state = ClusterState::BAD;
            } else if (FAT32Entry::isReserved(value)) {
                state = ClusterState::RESERVED;
            } else if (FAT32Entry::isEndOfChain(value)) {
                state = ClusterState::END_OF_CHAIN;
            } else {
                state = ClusterState::ALLOCATED;
                nextCluster = value;
            }
            break;
        }
        default:
            state = ClusterState::UNKNOWN;
            break;
    }

    return FATClusterInfo(clusterIndex, state, nextCluster, true);
}

bool FATTable::isBadCluster(uint32_t clusterIndex) const {
    if (!isValidClusterNumber(clusterIndex, clusterCount_)) {
        return false;
    }

    FATEntry entry = getEntry(clusterIndex);
    if (!isValidEntry(entry, clusterIndex)) {
        return false;
    }

    switch (entry.getType()) {
        case FATType::FAT12:
            return FAT12Entry::isBad(static_cast<uint16_t>(entry.getValue()));
        case FATType::FAT16:
            return FAT16Entry::isBad(static_cast<uint16_t>(entry.getValue()));
        case FATType::FAT32:
            return FAT32Entry::isBad(entry.getValue());
        default:
            return false;
    }
}

bool FATTable::validate() const {
    try {
        loadFatData();

        // Basic validation: check that we can read entries without crashing
        for (uint32_t i = 0; i < clusterCount_; ++i) {
            FATEntry entry = getEntry(i);
            if (!isValidEntry(entry, i)) {
                return false;
            }
        }

        // Additional validation could be added here
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

FATTable::Statistics FATTable::getStatistics() const {
    Statistics stats;
    stats.totalClusters = clusterCount_;

    for (uint32_t i = 0; i < clusterCount_; ++i) {
        FATClusterInfo info = getClusterState(i);
        if (!info.isValid) {
            continue;
        }

        switch (info.state) {
            case ClusterState::FREE:
                ++stats.freeClusters;
                break;
            case ClusterState::ALLOCATED:
                ++stats.allocatedClusters;
                break;
            case ClusterState::BAD:
                ++stats.badClusters;
                break;
            case ClusterState::RESERVED:
                ++stats.reservedClusters;
                break;
            case ClusterState::END_OF_CHAIN:
                ++stats.endOfChainClusters;
                break;
            case ClusterState::UNKNOWN:
                // Don't count unknown states in statistics
                break;
        }
    }

    return stats;
}

std::vector<uint32_t> FATTable::followClusterChain(uint32_t startCluster) const {
    std::vector<uint32_t> chain;
    std::unordered_set<uint32_t> visited;  // To detect cycles

    if (!isValidClusterNumber(startCluster, clusterCount_)) {
        return chain;  // Empty chain for invalid start
    }

    uint32_t currentCluster = startCluster;

    while (isValidClusterNumber(currentCluster, clusterCount_)) {
        // Check for cycles
        if (visited.find(currentCluster) != visited.end()) {
            // Cycle detected - stop here to prevent infinite loop
            break;
        }
        visited.insert(currentCluster);

        // Add current cluster to chain
        chain.push_back(currentCluster);

        // Get cluster state
        FATClusterInfo info = getClusterState(currentCluster);
        if (!info.isValid) {
            break;  // Invalid cluster - stop chain
        }

        // Check if we've reached end of chain
        if (info.state == ClusterState::END_OF_CHAIN ||
            info.state == ClusterState::BAD ||
            info.state == ClusterState::FREE ||
            info.state == ClusterState::RESERVED) {
            break;
        }

        // Move to next cluster
        currentCluster = info.nextCluster;

        // Prevent infinite loop with reasonable chain length limit
        if (chain.size() > clusterCount_ * 2) {
            break;
        }
    }

    return chain;
}

bool FATTable::isValidClusterChain(uint32_t startCluster) const {
    std::vector<uint32_t> chain = followClusterChain(startCluster);

    // A valid chain should:
    // 1. Start with a valid cluster
    // 2. Not be empty (unless start cluster was invalid)
    // 3. End with END_OF_CHAIN, BAD, FREE, or RESERVED state
    // 4. Not have detected cycles (handled in followClusterChain by stopping early)

    if (!isValidClusterNumber(startCluster, clusterCount_)) {
        return false;
    }

    if (chain.empty()) {
        return false;
    }

    // Check that the last cluster in the chain has a valid termination state
    uint32_t lastCluster = chain.back();
    FATClusterInfo lastInfo = getClusterState(lastCluster);

    if (!lastInfo.isValid) {
        return false;
    }

    // Valid ending states
    return (lastInfo.state == ClusterState::END_OF_CHAIN ||
            lastInfo.state == ClusterState::BAD ||
            lastInfo.state == ClusterState::FREE ||
            lastInfo.state == ClusterState::RESERVED);
}

void FATTable::loadFatData() const {
    if (fatDataLoaded_) {
        return;
    }

    // Calculate total size needed for FAT data
    uint64_t fatSizeBytes = static_cast<uint64_t>(sectorsPerFat_) * bytesPerSector_;
    if (fatSizeBytes > std::numeric_limits<size_t>::max()) {
        throw std::runtime_error("FAT table too large to allocate");
    }

    size_t fatSize = static_cast<size_t>(fatSizeBytes);
    fatData_.resize(fatSize);

    // Read each sector of the FAT
    for (uint32_t sectorIndex = 0; sectorIndex < sectorsPerFat_; ++sectorIndex) {
        uint64_t sectorOffset = getFatSectorOffset(sectorIndex);
        uint8_t* sectorDest = fatData_.data() + (sectorIndex * bytesPerSector_);

        uint32_t bytesRead = reader_->readSector(sectorOffset, sectorDest, bytesPerSector_);
        if (bytesRead != bytesPerSector_) {
            throw std::runtime_error("Failed to read FAT sector");
        }
    }

    fatDataLoaded_ = true;
}

uint64_t FATTable::getFatSectorOffset(uint32_t fatSectorIndex) const {
    // The FAT starts right after the reserved sectors
    return static_cast<uint64_t>(reservedSectorCount_ + fatSectorIndex) * bytesPerSector_;
}

bool FATTable::isValidEntry(const FATEntry& entry, uint32_t clusterIndex) const {
    if (entry.getType() == FATType::UNKNOWN) {
        return false;
    }

    // For FAT12, check that the value fits in 12 bits
    if (entry.getType() == FATType::FAT12 && entry.getValue() > 0xFFF) {
        return false;
    }

    // For FAT16, check that the value fits in 16 bits
    if (entry.getType() == FATType::FAT16 && entry.getValue() > 0xFFFF) {
        return false;
    }

    // For FAT32, check that the value fits in 28 bits (the upper 4 bits are reserved)
    if (entry.getType() == FATType::FAT32 && (entry.getValue() & 0xF0000000) != 0) {
        return false;
    }

    // Check that cluster index is valid
    return isValidClusterNumber(clusterIndex, clusterCount_);
}

uint32_t FATTable::readSectors(uint64_t sectorOffset, uint32_t sectorCount, uint8_t* buffer, uint32_t bytesPerSector) const {
    if (!reader_) {
        return 0;
    }
    return reader_->readSectors(sectorOffset, sectorCount, buffer, bytesPerSector);
}

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite