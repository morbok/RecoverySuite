#ifndef RECOVERYSUITE_FILESYSTEM_FAT_FATCLUSTERSTATE_H
#define RECOVERYSUITE_FILESYSTEM_FAT_FATCLUSTERSTATE_H

#include <cstdint>
#include <string>
#include "FATEntry.h"

namespace recoverysuite {
namespace filesystem {
namespace fat {

enum class ClusterState {
    FREE,           // Cluster is available for use
    ALLOCATED,      // Cluster is allocated to a file/directory
    BAD,            // Cluster is marked as bad
    RESERVED,       // Cluster is reserved (FAT metadata areas)
    END_OF_CHAIN,   // Cluster marks end of a file/directory chain
    UNKNOWN         // Cluster state could not be determined
};

struct FATClusterInfo {
    uint32_t clusterNumber;
    ClusterState state;
    uint32_t nextCluster;  // Valid if state is ALLOCATED and not end of chain
    bool isValid;          // Whether the cluster entry was read successfully

    FATClusterInfo() : clusterNumber(0), state(ClusterState::UNKNOWN),
                      nextCluster(0), isValid(false) {}

    FATClusterInfo(uint32_t clusterNum, ClusterState st, uint32_t nextClus = 0, bool valid = true)
        : clusterNumber(clusterNum), state(st), nextCluster(nextClus), isValid(valid) {}
};

// Convert cluster state to string for debugging/logging
inline std::string clusterStateToString(ClusterState state) {
    switch (state) {
        case ClusterState::FREE: return "FREE";
        case ClusterState::ALLOCATED: return "ALLOCATED";
        case ClusterState::BAD: return "BAD";
        case ClusterState::RESERVED: return "RESERVED";
        case ClusterState::END_OF_CHAIN: return "END_OF_CHAIN";
        case ClusterState::UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

// Determine if a cluster number is valid for the given filesystem
inline bool isValidClusterNumber(uint32_t clusterNum, uint32_t maxClusterCount) {
    // Cluster 0 and 1 are reserved in FAT12/FAT16, cluster 2 is first data cluster
    // In FAT32, cluster 0 is reserved, cluster 1 is unused, cluster 2 is first data cluster
    if (clusterNum < 2) {
        return true;  // Clusters 0 and 1 are always valid (reserved)
    }
    return clusterNum < maxClusterCount;
}

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FAT_FATCLUSTERSTATE_H