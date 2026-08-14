#include "FATValidator.h"
#include <sstream>
#include <algorithm>
#include "FATClusterState.h"

namespace recoverysuite {
namespace filesystem {
namespace fat {

FATValidator::FATValidator(std::shared_ptr<const FATTable> fatTable)
    : fatTable_(fatTable) {
    if (!fatTable_) {
        throw std::invalid_argument("FAT table cannot be null");
    }
}

bool FATValidator::validate() const {
    clearValidationErrors();
    validationErrorsComputed_ = true;

    // Validate each entry in the FAT
    uint32_t clusterCount = fatTable_->getClusterCount();
    for (uint32_t i = 0; i < clusterCount; ++i) {
        FATEntry entry = fatTable_->getEntry(i);
        if (!validateEntry(i, entry)) {
            // Error already added in validateEntry
        }
    }

    // Additionally, we can check for cluster chains that are referenced (if we had directory info)
    // But for now, we'll just return whether we found any errors
    return validationErrors_.empty();
}

bool FATValidator::validateClusterChain(uint32_t startCluster) const {
    clearValidationErrors();
    validationErrorsComputed_ = true;

    if (!isValidClusterNumber(startCluster)) {
        addValidationError(ValidationError::Type::CLUSTER_OUT_OF_RANGE, startCluster,
                           "Start cluster number is out of valid range");
        return false;
    }

    // Follow the chain and look for issues
    std::vector<uint32_t> chain = fatTable_->followClusterChain(startCluster);
    if (chain.empty()) {
        addValidationError(ValidationError::Type::CLUSTER_CHAIN_END_MISSING, startCluster,
                           "Cluster chain is empty or invalid");
        return false;
    }

    // Check for cycles (followClusterChain already stops at cycles, but we can check if the chain length is unreasonable)
    // We'll also check that the chain ends properly
    uint32_t lastCluster = chain.back();
    FATEntry lastEntry = fatTable_->getEntry(lastCluster);
    if (!fatTable_->isValidEntry(lastEntry, lastCluster) ||
        !(fatTable_->getClusterState(lastCluster).state == ClusterState::END_OF_CHAIN ||
          fatTable_->getClusterState(lastCluster).state == ClusterState::BAD ||
          fatTable_->getClusterState(lastCluster).state == ClusterState::FREE ||
          fatTable_->getClusterState(lastCluster).state == ClusterState::RESERVED)) {
        addValidationError(ValidationError::Type::CLUSTER_CHAIN_END_MISSING, lastCluster,
                           "Cluster chain does not end with a valid end-of-chain marker");
        return false;
    }

    return validationErrors_.empty();
}

std::vector<FATValidator::ValidationError> FATValidator::getValidationErrors() const {
    if (!validationErrorsComputed_) {
        // If validation hasn't been run, run it now
        validate();
    }
    return validationErrors_;
}

void FATValidator::clearValidationErrors() const {
    validationErrors_.clear();
    validationErrorsComputed_ = false;
}

void FATValidator::addValidationError(ValidationError::Type type, uint32_t clusterNumber, const std::string& description) const {
    validationErrors_.push_back({type, clusterNumber, description});
}

bool FATValidator::validateEntry(uint32_t clusterIndex, const FATEntry& entry) const {
    if (!fatTable_->isValidEntry(entry, clusterIndex)) {
        std::stringstream ss;
        ss << "Invalid FAT entry for cluster " << clusterIndex << ": value 0x"
           << std::hex << entry.getValue();
        addValidationError(ValidationError::Type::INVALID_ENTRY, clusterIndex, ss.str());
        return false;
    }

    // Check if the cluster is reserved and marked as allocated (or vice versa)
    ClusterState state = fatTable_->getClusterState(clusterIndex).state;
    if (isReservedCluster(clusterIndex, entry)) {
        if (state != ClusterState::RESERVED) {
            std::stringstream ss;
            ss << "Reserved cluster " << clusterIndex << " is not marked as reserved (state: "
               << static_cast<int>(state) << ")";
            addValidationError(ValidationError::Type::RESERVED_CLUSTER_USED, clusterIndex, ss.str());
            return false;
        }
    } else {
        // Non-reserved clusters should not be marked as reserved
        if (state == ClusterState::RESERVED) {
            std::stringstream ss;
            ss << "Non-reserved cluster " << clusterIndex << " is marked as reserved";
            addValidationError(ValidationError::Type::RESERVED_CLUSTER_USED, clusterIndex, ss.str());
            return false;
        }
    }

    // Check for bad clusters marked as allocated
    if (fatTable_->isBadCluster(clusterIndex)) {
        if (state == ClusterState::ALLOCATED) {
            std::stringstream ss;
            ss << "Bad cluster " << clusterIndex << " is marked as allocated";
            addValidationError(ValidationError::Type::BAD_CLUSTER_MARKED_ALLOCATED, clusterIndex, ss.str());
            return false;
        }
    }

    return true;
}

bool FATValidator::isValidClusterNumber(uint32_t clusterNumber) const {
    return recoverysuite::filesystem::fat::isValidClusterNumber(clusterNumber, fatTable_->getClusterCount());
}

bool FATValidator::isReservedCluster(uint32_t /*clusterNumber*/, const FATEntry& entry) const {
    switch (fatTable_->getFatType()) {
        case FATType::FAT12:
            // In FAT12, clusters 0xFF0 to 0xFF6 are reserved
            return (entry.getValue() >= 0xFF0 && entry.getValue() <= 0xFF6);
        case FATType::FAT16:
            // In FAT16, clusters 0xFFF0 to 0xFFFE are reserved
            return (entry.getValue() >= 0xFFF0 && entry.getValue() <= 0xFFFE);
        case FATType::FAT32:
            // In FAT32, clusters 0x0FFFFFF0 to 0x0FFFFFF6 are reserved
            return (entry.getValue() >= 0x0FFFFFF0 && entry.getValue() <= 0x0FFFFFF6);
        default:
            return false;
    }
}

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite