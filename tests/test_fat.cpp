#include <memory>
#include <iostream>
#include <cassert>
#include <cstring>
#include "Filesystem/FAT/FATFilesystem.h"
#include "Filesystem/FAT/FATConstants.h"
#include "Filesystem/FAT/FATStructures.h"
#include "Filesystem/FAT/FATReader.h"
#include "Filesystem/FAT/FATMetadata.h"
#include "Filesystem/FAT/FATAnalyzer.h"
#include "Filesystem/FAT/FATVolume.h"
#include "Filesystem/FilesystemTypes.h"

using namespace recoverysuite::filesystem::fat;
using recoverysuite::filesystem::FilesystemCapabilities;

// Mock reader for testing
class MockFATReader : public FATReader {
public:
    MockFATReader(uint64_t startOffset, uint32_t sectorSize, const FATBootSectorCommon& bootSectorData)
        : FATReader(startOffset, sectorSize), bootSectorData_(bootSectorData) {
    }

    uint32_t readSector(uint64_t sectorIndex, uint8_t* buffer, uint32_t sectorSize) override {
        if (sectorIndex != 0) {
            return 0; // Only sector 0 is valid for this mock
        }
        if (sectorSize < sizeof(FATBootSectorCommon)) {
            return 0;
        }
        std::memcpy(buffer, &bootSectorData_, sizeof(FATBootSectorCommon));
        return sizeof(FATBootSectorCommon);
    }

    uint32_t readSectors(uint64_t startSector, uint32_t sectorCount, uint8_t* buffer, uint32_t sectorSize) override {
        (void)startSector;
        (void)sectorCount;
        (void)buffer;
        (void)sectorSize;
        return 0; // Not implemented for this test
    }

    uint32_t readCluster(uint64_t clusterIndex, uint8_t* buffer, uint32_t clusterSize) override {
        (void)clusterIndex;
        (void)buffer;
        (void)clusterSize;
        return 0; // Not implemented for this test
    }

    uint32_t readClusters(uint64_t startCluster, uint32_t clusterCount, uint8_t* buffer, uint32_t clusterSize) override {
        (void)startCluster;
        (void)clusterCount;
        (void)buffer;
        (void)clusterSize;
        return 0; // Not implemented for this test
    }

    uint64_t getTotalSectors() const override {
        return 0; // Not implemented for this test
    }

    uint32_t getSectorSize() const override {
        return 512; // Default sector size
    }

private:
    FATBootSectorCommon bootSectorData_;
};

void test_fat12_boot_sector() {
    // Create a mock FAT12 boot sector
    FATBootSectorCommon bootSector = {};
    bootSector.jumpInstruction[0] = 0xEB;
    bootSector.jumpInstruction[1] = 0x3C;
    bootSector.jumpInstruction[2] = 0x90;
    std::memcpy(bootSector.oemName, "MSWIN4.1", 8);

    bootSector.bytesPerSector = 512;
    bootSector.sectorsPerCluster = 1;
    bootSector.reservedSectorCount = 1;
    bootSector.fatCount = 2;
    bootSector.rootEntryCount = 224;
    bootSector.totalSectors16 = 2880; // 1.44 MB floppy
    bootSector.mediaType = 0xF0;
    bootSector.sectorsPerFat16 = 9;
    bootSector.sectorsPerTrack = 18;
    bootSector.headCount = 2;
    bootSector.hiddenSectors = 0;
    bootSector.totalSectors32 = 0;

    // FAT12 extended BPB
    bootSector.fat16.driveNumber = 0x00;
    bootSector.fat16.reserved1 = 0;
    bootSector.fat16.bootSignature = 0x29;
    bootSector.fat16.volumeID = 0x12345678;
    std::memcpy(bootSector.fat16.volumeLabel, "NO NAME    ", 11);
    std::memcpy(bootSector.fat16.fatTypeLabel, "FAT12   ", 8);

    bootSector.bootSignature = FAT_BOOT_SIGNATURE;


    // Test FATFilesystem
    FATFilesystem fs(std::make_shared<MockFATReader>(0, 512, bootSector));

    // Test getType
    assert(fs.getType() == FilesystemType::FAT12);

    // Test getName
    assert(fs.getName() == "FAT12");

    // Test getVersion
    assert(fs.getVersion() == "");

    // Test supportsCapability
    assert(fs.supportsCapability(FilesystemCapability::READ_SUPPORT) == true);
    assert(fs.supportsCapability(FilesystemCapability::WRITE_SUPPORT) == false);
    assert(fs.supportsCapability(FilesystemCapability::METADATA_SUPPORT) == true);

    // Test getCapabilities
    FilesystemCapabilities caps = fs.getCapabilities();
    assert((caps & FilesystemCapability::READ_SUPPORT) != 0);
    assert((caps & FilesystemCapability::METADATA_SUPPORT) != 0);
    assert((caps & FilesystemCapability::WRITE_SUPPORT) == 0);

    // Test createReader
    auto fsReader = fs.createReader();
    assert(fsReader != nullptr);

    // Test createAnalyzer
    auto fsAnalyzer = fs.createAnalyzer();
    assert(fsAnalyzer != nullptr);

    // Test getMetadata
    auto fsMetadata = fs.getMetadata();
    assert(fsMetadata != nullptr);
    assert(fsMetadata->getType() == FilesystemType::FAT12);
    assert(fsMetadata->getName() == "FAT12");
    assert(fsMetadata->getBytesPerSector() == 512);
    assert(fsMetadata->getSectorsPerCluster() == 1);
    assert(fsMetadata->getReservedSectorCount() == 1);
    assert(fsMetadata->getFatCount() == 2);
    assert(fsMetadata->getRootEntryCount() == 224);

    // Test getVolume
    auto fsVolume = fs.getVolume();
    assert(fsVolume != nullptr);
    assert(fsVolume->getFilesystemType() == FilesystemType::FAT12);
    assert(fsVolume->isBootable() == true);

    std::cout << "[PASS] FAT12 boot sector test passed" << std::endl;
}

void test_fat16_boot_sector() {
    // Create a mock FAT16 boot sector
    FATBootSectorCommon bootSector = {};
    bootSector.jumpInstruction[0] = 0xEB;
    bootSector.jumpInstruction[1] = 0x3C;
    bootSector.jumpInstruction[2] = 0x90;
    std::memcpy(bootSector.oemName, "MSWIN4.1", 8);

    bootSector.bytesPerSector = 512;
    bootSector.sectorsPerCluster = 4;
    bootSector.reservedSectorCount = 32;
    bootSector.fatCount = 2;
    bootSector.rootEntryCount = 512;
    bootSector.totalSectors16 = 65535; // Max value for 16-bit
    bootSector.mediaType = 0xF8;
    bootSector.sectorsPerFat16 = 16384;
    bootSector.sectorsPerTrack = 63;
    bootSector.headCount = 255;
    bootSector.hiddenSectors = 0;
    bootSector.totalSectors32 = 0;

    // FAT16 extended BPB
    bootSector.fat16.driveNumber = 0x80;
    bootSector.fat16.reserved1 = 0;
    bootSector.fat16.bootSignature = 0x29;
    bootSector.fat16.volumeID = 0x12345678;
    std::memcpy(bootSector.fat16.volumeLabel, "NO NAME    ", 11);
    std::memcpy(bootSector.fat16.fatTypeLabel, "FAT16   ", 8);

    bootSector.bootSignature = FAT_BOOT_SIGNATURE;


    // Test FATFilesystem
    FATFilesystem fs(std::make_shared<MockFATReader>(0, 512, bootSector));

    // Test getType
    assert(fs.getType() == FilesystemType::FAT16);

    // Test getName
    assert(fs.getName() == "FAT16");

    std::cout << "[PASS] FAT16 boot sector test passed" << std::endl;
}

void test_fat32_boot_sector() {
    // Create a mock FAT32 boot sector
    FATBootSectorCommon bootSector = {};
    bootSector.jumpInstruction[0] = 0xEB;
    bootSector.jumpInstruction[1] = 0x58;
    bootSector.jumpInstruction[2] = 0x90;
    std::memcpy(bootSector.oemName, "MSWIN4.1", 8);

    bootSector.bytesPerSector = 512;
    bootSector.sectorsPerCluster = 8;
    bootSector.reservedSectorCount = 32;
    bootSector.fatCount = 2;
    bootSector.rootEntryCount = 0; // For FAT32, root entry count is 0
    bootSector.totalSectors16 = 0; // For FAT32, this is 0
    bootSector.mediaType = 0xF8;
    bootSector.sectorsPerFat16 = 0; // For FAT32, this is 0
    bootSector.sectorsPerTrack = 63;
    bootSector.headCount = 255;
    bootSector.hiddenSectors = 0;
    bootSector.totalSectors32 = 2048000; // ~1 GB

    // FAT32 extended BPB
    bootSector.fat32.sectorsPerFat32 = 8192;
    bootSector.fat32.extFlags = 0;
    bootSector.fat32.fsVersion = 0;
    bootSector.fat32.rootCluster = 2;
    bootSector.fat32.fsInfoSector = 1;
    bootSector.fat32.backupBootSector = 6;
    std::memcpy(bootSector.fat32.reserved2, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 12);
    bootSector.fat32.driveNumber = 0x80;
    bootSector.fat32.reserved3 = 0;
    bootSector.fat32.bootSignature = 0x29;
    bootSector.fat32.volumeID = 0x12345678;
    std::memcpy(bootSector.fat32.volumeLabel, "NO NAME    ", 11);
    std::memcpy(bootSector.fat32.fatTypeLabel, "FAT32   ", 8);

    bootSector.bootSignature = FAT_BOOT_SIGNATURE;


    // Test FATFilesystem
    FATFilesystem fs(std::make_shared<MockFATReader>(0, 512, bootSector));

    // Test getType
    assert(fs.getType() == FilesystemType::FAT32);

    // Test getName
    assert(fs.getName() == "FAT32");

    std::cout << "[PASS] FAT32 boot sector test passed" << std::endl;
}

int main() {
    std::cout << "Testing RecoverySuite FAT Filesystem Module..." << std::endl;

    try {
        test_fat12_boot_sector();
        test_fat16_boot_sector();
        test_fat32_boot_sector();

        std::cout << "All FAT filesystem tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}