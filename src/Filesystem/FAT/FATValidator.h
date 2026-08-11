#ifndef RECOVERYSUITE_FILESYSTEM_FAT_FATVALIDATOR_H
#define RECOVERYSUITE_FILESYSTEM_FAT_FATVALIDATOR_H

#include <cstdint>
#include <vector>
#include <string>
#include "FATTable.h"
#include "FATEntry.h"

namespace recoverysuite {
namespace filesystem {
namespace fat {

/**
 * FAT Validator
 * Validates FAT table data for consistency and correctness
 */
class FATValidator {
public:
    // Constructor
    explicit FATValidator(std::shared_ptr<const FATTable> fatTable);

    // Destructor
    ~FATValidator() = default;

    // Validate the entire FAT table
    bool validate() const;

    // Validate a specific cluster chain
    bool validateClusterChain(uint32_t startCluster) const;

    // Check for free clusters that are marked as allocated (or vice versa) by cross-checking with directory entries
    // This would require directory information, so we'll leave it for a more integrated validator
    // For now, we'll focus on internal FAT consistency

    // Get a list of validation errors
    struct ValidationError {
        enum class Type {
            INVALID_ENTRY,
            CLUSTER_OUT_OF_RANGE,
            RESERVED_CLUSTER_USED,
            BAD_CLUSTER_MARKED_ALLOCATED,
            CLUSTER_CHAIN_CYCLE,
            CLUSTER_CHAIN_END_MISSING,
            CLUSTER_COUNT_MISMATCH
        };

        Type type;
        uint32_t clusterNumber;
        std::string description;
    };

    std::vector<ValidationError> getValidationErrors() const;

private:
    // FAT table to validate
    std::shared_ptr<const FATTable> fatTable_;

    // Collect validation errors
    mutable std::vector<ValidationError> validationErrors_;
    mutable bool validationErrorsComputed_ = false;

    // Clear validation errors
    void clearValidationErrors() const;

    // Add a validation error
    void addValidationError(ValidationError::Type type, uint32_t clusterNumber, const std::string& description) const;

    // Validate a single FAT entry
    bool validateEntry(uint32_t clusterIndex, const FATEntry& entry) const;

    // Validate that a cluster number is within the valid range for the FAT type
    bool isValidClusterNumber(uint32_t clusterNumber) const;

    // Check if a cluster is marked as reserved in the FAT (according to FAT specification)
    bool isReservedCluster(uint32_t clusterNumber, const FATEntry& entry) const;
};

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FAT_FATVALIDATOR_H