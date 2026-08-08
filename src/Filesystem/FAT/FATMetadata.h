#ifndef RECOVERYSUITE_FILESYSTEM_FAT_FATMETADATA_H
#define RECOVERYSUITE_FILESYSTEM_FAT_FATMETADATA_H

#include <memory>
#include <string>
#include "../FilesystemMetadata.h"
#include "../FilesystemReader.h"
#include "FATConstants.h"
#include "FATStructures.h"

namespace recoverysuite {
namespace filesystem {
namespace fat {

// FAT Metadata provides metadata about a FAT filesystem.
class FATMetadata : public FilesystemMetadata {
public:
    // Constructor
    explicit FATMetadata(FilesystemReader& reader);

    // Destructor
    ~FATMetadata() override = default;

    // Get the type of this filesystem
    FilesystemType getType() const override;

    // Get the name of this filesystem (e.g., "FAT12", "FAT16", "FAT32")
    std::string getName() const override;

    // Get the version of this filesystem (if applicable)
    std::string getVersion() const override;

    // Get the serial number or unique identifier (if applicable)
    std::string getSerialNumber() const override;

    // Get the volume serial number as a 32-bit integer (FAT-specific)
    uint32_t getVolumeSerialNumber() const;

    // Get the volume label (if applicable)
    std::string getVolumeLabel() const override;

    // Get the FAT type (FAT12, FAT16, FAT32)
    fat::FATType getFatType() const;

    // Get the bytes per sector
    uint16_t getBytesPerSector() const;

    // Get the sectors per cluster
    uint8_t getSectorsPerCluster() const;

    // Get the reserved sector count
    uint16_t getReservedSectorCount() const;

    // Get the number of FAT copies
    uint8_t getFatCount() const;

    // Get the root entry count (for FAT12/FAT16)
    uint16_t getRootEntryCount() const;

    // Get the total sectors (16-bit)
    uint16_t getTotalSectors16() const;

    // Get the total sectors (32-bit)
    uint32_t getTotalSectors32() const;

    // Get the sectors per FAT (for FAT12/FAT16)
    uint16_t getSectorsPerFat16() const;

    // Get the sectors per FAT (for FAT32)
    uint32_t getSectorsPerFat32() const;

    // Get the total size in bytes
    uint64_t getTotalSize() const override;

    // Get the free space in bytes
    uint64_t getFreeSpace() const override;

    // Get the used space in bytes
    uint64_t getUsedSpace() const override;

    // Get the block size (sector size) in bytes
    uint32_t getBlockSize() const override;

    // Get the number of blocks
    uint64_t getBlockCount() const override;

    // Get the number of free blocks
    uint64_t getFreeBlockCount() const override;

    // Check if the filesystem is read-only
    bool isReadOnly() const override;

    // Check if the filesystem is mounted
    bool isMounted() const override;

protected:
    // FAT boot sector data
    FATBootSectorCommon bootSector_;

    // FAT type
    fat::FATType fatType_;

    // Initialize the metadata by reading the boot sector
    void initialize(FilesystemReader& reader);

private:
    // Calculate FAT type based on cluster count
    FATType calculateFatType(uint32_t clusterCount) const;
};

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FAT_FATMETADATA_H