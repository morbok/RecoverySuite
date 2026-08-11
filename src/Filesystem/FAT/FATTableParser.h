#ifndef RECOVERYSUITE_FILESYSTEM_FAT_FATTABLEPARSER_H
#define RECOVERYSUITE_FILESYSTEM_FAT_FATTABLEPARSER_H

#include <cstdint>
#include <memory>
#include "FATTable.h"
#include "../FilesystemReader.h"

namespace recoverysuite {
namespace filesystem {
namespace fat {

/**
 * Parses FAT tables from disk structures
 * Extracts FAT parameters from boot sector and creates FATTable instances
 */
class FATTableParser {
public:
    // Constructor
    explicit FATTableParser(std::shared_ptr<FilesystemReader> reader);

    // Destructor
    ~FATTableParser() = default;

    // Parse FAT table parameters from boot sector and create FATTable
    std::shared_ptr<FATTable> parseFatTable() const;

    // Get individual FAT parameters (useful for testing/debugging)
    uint32_t getSectorsPerFat() const;
    uint32_t getBytesPerSector() const;
    uint32_t getClusterCount() const;
    FATType getFatType() const;
    uint32_t getFatCount() const;
    uint32_t getReservedSectorCount() const;

private:
    // Reader for accessing the disk
    std::shared_ptr<FilesystemReader> reader_;

    // Cached boot sector data
    mutable struct FATBootSectorCommon bootSector_;
    mutable bool bootSectorLoaded_ = false;

    // Load boot sector from disk
    void loadBootSector() const;

    // Validate boot sector
    bool validateBootSector() const;

    // Calculate FAT parameters from boot sector
    void calculateFatParameters(uint32_t& sectorsPerFat, uint32_t& bytesPerSector,
                               uint32_t& clusterCount, FATType& fatType,
                               uint32_t& fatCount, uint32_t& reservedSectorCount) const;
};

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FAT_FATTABLEPARSER_H