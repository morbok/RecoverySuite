#include "FATFilesystem.h"
#include <memory>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include "FATReader.h"
#include "FATAnalyzer.h"
#include "FATMetadata.h"
#include "FATVolume.h"

namespace recoverysuite {
namespace filesystem {
namespace fat {

FATFilesystem::FATFilesystem(uint64_t startOffset, uint32_t sectorSize)
    : startOffset_(startOffset), sectorSize_(sectorSize), reader_(nullptr), initialized_(false) {
    if (sectorSize_ == 0) {
        throw std::invalid_argument("Sector size must be greater than zero");
    }
}

FATFilesystem::FATFilesystem(std::shared_ptr<FilesystemReader> reader)
    : startOffset_(reader->getStartOffset()), sectorSize_(reader->getSectorSize()), reader_(reader), initialized_(false) {
    if (!reader_) {
        throw std::invalid_argument("Reader cannot be null");
    }
    if (sectorSize_ == 0) {
        throw std::invalid_argument("Sector size must be greater than zero");
    }
}

FilesystemType FATFilesystem::getType() const {
    initialize();
    switch (fatType_) {
        case fat::FATType::FAT12: return FilesystemType::FAT12;
        case fat::FATType::FAT16: return FilesystemType::FAT16;
        case fat::FATType::FAT32: return FilesystemType::FAT32;
        default: return FilesystemType::UNKNOWN;
    }
}

std::string FATFilesystem::getName() const {
    initialize();
    return fat::fatTypeToString(fatType_);
}

std::string FATFilesystem::getVersion() const {
    // FAT filesystems don't have a version in the traditional sense
    return "";
}

bool FATFilesystem::supportsCapability(FilesystemCapability capability) const {
    // FAT filesystems support read support and maybe metadata support
    switch (capability) {
        case FilesystemCapability::READ_SUPPORT:
            return true;
        case FilesystemCapability::METADATA_SUPPORT:
            return true;
        default:
            return false;
    }
}

FilesystemCapabilities FATFilesystem::getCapabilities() const {
    FilesystemCapabilities caps =
        static_cast<FilesystemCapabilities>(FilesystemCapability::READ_SUPPORT) |
        static_cast<FilesystemCapabilities>(FilesystemCapability::METADATA_SUPPORT);
    return caps;
}

std::unique_ptr<FilesystemReader> FATFilesystem::createReader() const {
    initialize();
    if (reader_) {
        // Return a copy of the existing reader for testing
        return std::make_unique<FATReader>(reader_->getStartOffset(), reader_->getSectorSize());
    }
    return std::make_unique<FATReader>(startOffset_, sectorSize_);
}

std::unique_ptr<FilesystemAnalyzer> FATFilesystem::createAnalyzer() const {
    initialize();
    // Create a temporary reader that reads from the cached boot sector.
    struct BootSectorReader : public FilesystemReader {
        BootSectorReader(const FATBootSectorCommon& bootSector, uint64_t startOffset, uint32_t sectorSize)
            : bootSector_(bootSector), startOffset_(startOffset), sectorSize_(sectorSize) {}
        uint32_t readSector(uint64_t sectorIndex, uint8_t* buffer, uint32_t sectorSize) override {
            if (sectorIndex != 0) {
                return 0;
            }
            if (sectorSize < sizeof(FATBootSectorCommon)) {
                return 0;
            }
            std::memcpy(buffer, &bootSector_, sizeof(FATBootSectorCommon));
            return sizeof(FATBootSectorCommon);
        }
        uint32_t readSectors(uint64_t startSector, uint32_t sectorCount, uint8_t* buffer, uint32_t sectorSize) override {
            (void)startSector; (void)sectorCount; (void)buffer; (void)sectorSize;
            return 0;
        }
        uint32_t readCluster(uint64_t clusterIndex, uint8_t* buffer, uint32_t clusterSize) override {
            (void)clusterIndex; (void)buffer; (void)clusterSize;
            return 0;
        }
        uint32_t readClusters(uint64_t startCluster, uint32_t clusterCount, uint8_t* buffer, uint32_t clusterSize) override {
            (void)startCluster; (void)clusterCount; (void)buffer; (void)clusterSize;
            return 0;
        }
        uint64_t getTotalSectors() const override {
            return 0; // We don't have this information without the FAT table.
        }
        uint32_t getSectorSize() const override {
            return sectorSize_;
        }
        uint64_t getStartOffset() const override {
            return startOffset_;
        }
        void flush() override {}
        
        FATBootSectorCommon bootSector_;
        uint64_t startOffset_;
        uint32_t sectorSize_;
    } bootSectorReader(bootSector_, startOffset_, sectorSize_);
    return std::make_unique<FATAnalyzer>(bootSectorReader);
}

std::unique_ptr<FilesystemMetadata> FATFilesystem::getMetadata() const {
    initialize();
    // Create a temporary reader that reads from the cached boot sector.
    struct BootSectorReader : public FilesystemReader {
        BootSectorReader(const FATBootSectorCommon& bootSector, uint64_t startOffset, uint32_t sectorSize)
            : bootSector_(bootSector), startOffset_(startOffset), sectorSize_(sectorSize) {}
        uint32_t readSector(uint64_t sectorIndex, uint8_t* buffer, uint32_t sectorSize) override {
            if (sectorIndex != 0) {
                return 0;
            }
            if (sectorSize < sizeof(FATBootSectorCommon)) {
                return 0;
            }
            std::memcpy(buffer, &bootSector_, sizeof(FATBootSectorCommon));
            return sizeof(FATBootSectorCommon);
        }
        uint32_t readSectors(uint64_t startSector, uint32_t sectorCount, uint8_t* buffer, uint32_t sectorSize) override {
            (void)startSector; (void)sectorCount; (void)buffer; (void)sectorSize;
            return 0;
        }
        uint32_t readCluster(uint64_t clusterIndex, uint8_t* buffer, uint32_t clusterSize) override {
            (void)clusterIndex; (void)buffer; (void)clusterSize;
            return 0;
        }
        uint32_t readClusters(uint64_t startCluster, uint32_t clusterCount, uint8_t* buffer, uint32_t clusterSize) override {
            (void)startCluster; (void)clusterCount; (void)buffer; (void)clusterSize;
            return 0;
        }
        uint64_t getTotalSectors() const override {
            return 0; // We don't have this information without the FAT table.
        }
        uint32_t getSectorSize() const override {
            return sectorSize_;
        }
        uint64_t getStartOffset() const override {
            return startOffset_;
        }
        void flush() override {}
        
        FATBootSectorCommon bootSector_;
        uint64_t startOffset_;
        uint32_t sectorSize_;
    } bootSectorReader(bootSector_, startOffset_, sectorSize_);
    return std::make_unique<FATMetadata>(bootSectorReader);
}

std::unique_ptr<FilesystemVolume> FATFilesystem::getVolume() const {
    initialize();
    return std::make_unique<FATVolume>(startOffset_, sectorSize_, bootSector_);
}

void FATFilesystem::initialize() const {
    if (initialized_) {
        return;
    }
    
    // We'll read the boot sector to determine the FAT type
    std::shared_ptr<FilesystemReader> readerToUse;
    if (reader_) {
        readerToUse = reader_;
    } else {
        readerToUse = std::make_shared<FATReader>(startOffset_, sectorSize_);
    }
    
    uint8_t buffer[512] = {0};
    uint32_t bytesRead = readerToUse->readSector(0, buffer, sectorSize_);
    if (bytesRead < sizeof(FATBootSectorCommon)) {
        throw std::runtime_error("Failed to read FAT boot sector");
    }
    std::memcpy(&bootSector_, buffer, sizeof(FATBootSectorCommon));

    // Validate the boot sector signature
    if (bootSector_.bootSignature != FAT_BOOT_SIGNATURE) {
        throw std::runtime_error("Invalid FAT boot sector signature");
    }

    // Determine the FAT type based on the cluster count
    uint32_t totalSectors = (bootSector_.totalSectors16 != 0) ? bootSector_.totalSectors16 : bootSector_.totalSectors32;
    uint32_t reservedSectors = bootSector_.reservedSectorCount;
    uint32_t fatsSize = bootSector_.fatCount *
        ( (bootSector_.fatCount == 2 && bootSector_.sectorsPerFat16 != 0) ? bootSector_.sectorsPerFat16 : 0 ); // Simplified
    uint32_t rootDirSectors = ( (bootSector_.rootEntryCount * 32) + (bootSector_.bytesPerSector - 1) ) / bootSector_.bytesPerSector;
    uint32_t dataSectors = totalSectors - (reservedSectors + fatsSize + rootDirSectors);
    uint32_t clusterCount = dataSectors / bootSector_.sectorsPerCluster;

    if (clusterCount < fat::FAT12_MAX_CLUSTERS) {
        fatType_ = fat::FATType::FAT12;
    } else if (clusterCount < fat::FAT16_MAX_CLUSTERS) {
        fatType_ = fat::FATType::FAT16;
    } else {
        fatType_ = fat::FATType::FAT32;
    }

    initialized_ = true;
}

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite
