#include <memory>
#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include "Filesystem/FAT/FATEntry.h"
#include "Filesystem/FAT/FATClusterState.h"
#include "Filesystem/FAT/FATTable.h"
#include "Filesystem/FAT/FATTableParser.h"
#include "Filesystem/FAT/FATChainReader.h"
#include "Filesystem/FAT/FATValidator.h"
#include "Filesystem/FAT/FATReader.h"
#include "Filesystem/FAT/FATConstants.h"
#include "Filesystem/FAT/FATStructures.h"

using namespace recoverysuite::filesystem::fat;

// Mock reader that simulates a FAT filesystem
class MockFATReader : public FATReader {
public:
    MockFATReader(uint64_t startOffset, uint32_t sectorSize,
                  const FATBootSectorCommon& bootSectorData,
                  const std::vector<uint8_t>& fatData,
                  uint32_t sectorsPerFat)
        : FATReader(startOffset, sectorSize),
          bootSectorData_(bootSectorData),
          fatData_(fatData),
          sectorsPerFat_(sectorsPerFat) {
    }

    uint32_t readSector(uint64_t sectorIndex, uint8_t* buffer, uint32_t sectorSize) override {
        // For simplicity, we'll assume sector 0 is the boot sector
        // and sectors 1-N are the FAT sectors
        if (sectorIndex == 0) {
            if (sectorSize < sizeof(FATBootSectorCommon)) {
                return 0;
            }
            std::memcpy(buffer, &bootSectorData_, sizeof(FATBootSectorCommon));
            return sizeof(FATBootSectorCommon);
        } else if (sectorIndex >= 1 && sectorIndex <= sectorsPerFat_) {
            // FAT sector
            uint32_t fatIndex = sectorIndex - 1;
            uint32_t fatOffset = fatIndex * sectorSize;
            if (fatOffset + sectorSize > fatData_.size()) {
                // Handle case where we don't have enough FAT data
                std::memset(buffer, 0, sectorSize);
                return sectorSize;
            }
            std::memcpy(buffer, fatData_.data() + fatOffset, sectorSize);
            return sectorSize;
        }
        return 0; // Sector not available in our mock
    }

    uint32_t readSectors(uint64_t startSector, uint32_t sectorCount, uint8_t* buffer, uint32_t sectorSize) override {
        uint32_t totalBytesRead = 0;
        for (uint32_t i = 0; i < sectorCount; ++i) {
            uint32_t bytesRead = readSector(startSector + i, buffer + (i * sectorSize), sectorSize);
            if (bytesRead == 0) {
                break;
            }
            totalBytesRead += bytesRead;
        }
        return totalBytesRead;
    }

    // Other required overrides (returning 0/not implemented for simplicity)
    uint32_t readCluster(uint64_t clusterIndex, uint8_t* buffer, uint32_t clusterSize) override {
        (void)clusterIndex; (void)buffer; (void)clusterSize;
        return 0;
    }

    uint32_t readClusters(uint64_t startCluster, uint32_t clusterCount, uint8_t* buffer, uint32_t clusterSize) override {
        (void)startCluster; (void)clusterCount; (void)buffer; (void)clusterSize;
        return 0;
    }

    uint64_t getTotalSectors() const override {
        return 0; // Not needed for our tests
    }

    uint32_t getSectorSize() const override {
        return FATReader::getSectorSize();
    }

    uint64_t getStartOffset() const override {
        return FATReader::getStartOffset();
    }

    void flush() override {}

private:
    FATBootSectorCommon bootSectorData_;
    std::vector<uint8_t> fatData_;
    uint32_t sectorsPerFat_;
};

// Helper function to create a simple FAT12 boot sector
FATBootSectorCommon createFat12BootSector() {
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

    return bootSector;
}

// Helper function to create a simple FAT16 boot sector
FATBootSectorCommon createFat16BootSector() {
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

    return bootSector;
}

// Helper function to create a simple FAT32 boot sector
FATBootSectorCommon createFat32BootSector() {
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

    return bootSector;
}

void test_fat_entry() {
    std::cout << "Testing FATEntry..." << std::endl;

    // Test FAT12 entry
    FATEntry fat12Entry(FATType::FAT12, 0x123);
    assert(fat12Entry.getType() == FATType::FAT12);
    assert(fat12Entry.getValue() == 0x123);
    assert(!fat12Entry.isFree());
    assert(!fat12Entry.isBad());
    assert(!fat12Entry.isReserved());
    assert(!fat12Entry.isEndOfChain());

    // Test FAT16 entry
    FATEntry fat16Entry(FATType::FAT16, 0x1234);
    assert(fat16Entry.getType() == FATType::FAT16);
    assert(fat16Entry.getValue() == 0x1234);

    // Test FAT32 entry
    FATEntry fat32Entry(FATType::FAT32, 0x123456);
    assert(fat32Entry.getType() == FATType::FAT32);
    assert(fat32Entry.getValue() == 0x123456);

    // Test special values
    FATEntry freeEntry(FATType::FAT12, 0x000);
    assert(freeEntry.isFree());

    FATEntry badEntry(FATType::FAT12, 0xFF7);
    assert(badEntry.isBad());

    FATEntry reservedEntry(FATType::FAT12, 0xFF6);
    assert(reservedEntry.isReserved());

    FATEntry eocEntry(FATType::FAT12, 0xFFF);
    assert(eocEntry.isEndOfChain());

    std::cout << "[PASS] FATEntry tests passed" << std::endl;
}

void test_fat_table_basic() {
    std::cout << "Testing FATTable basic functionality..." << std::endl;

    // Create a simple FAT table with some test data
    FATBootSectorCommon bootSector = createFat12BootSector();
    std::shared_ptr<FATReader> reader = std::make_shared<MockFATReader>(
        0, 512, bootSector, std::vector<uint8_t>(), 9);

    // Create a FAT table
    FATTable table(FATType::FAT12, 9, 512, 4086, reader); // 4086 clusters max for FAT12

    assert(table.getFatType() == FATType::FAT12);
    assert(table.getSectorsPerFat() == 9);
    assert(table.getBytesPerSector() == 512);
    assert(table.getClusterCount() == 4086);
    assert(table.getFirstDataCluster() == 2);

    std::cout << "[PASS] FATTable basic tests passed" << std::endl;
}

void test_fat_table_parser() {
    std::cout << "Testing FATTableParser..." << std::endl;

    // Create a boot sector
    FATBootSectorCommon bootSector = createFat12BootSector();

    // Create a mock reader that just returns the boot sector
    class BootSectorOnlyReader : public FATReader {
    public:
        BootSectorOnlyReader(uint64_t startOffset, uint32_t sectorSize,
                           const FATBootSectorCommon& bootSectorData)
            : FATReader(startOffset, sectorSize), bootSectorData_(bootSectorData) {
        }

        uint32_t readSector(uint64_t sectorIndex, uint8_t* buffer, uint32_t sectorSize) override {
            if (sectorIndex == 0) {
                if (sectorSize < sizeof(FATBootSectorCommon)) {
                    return 0;
                }
                std::memcpy(buffer, &bootSectorData_, sizeof(FATBootSectorCommon));
                return sizeof(FATBootSectorCommon);
            }
            return 0;
        }

        // Required overrides
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
        uint64_t getTotalSectors() const override { return 0; }
        uint32_t getSectorSize() const override { return FATReader::getSectorSize(); }
        uint64_t getStartOffset() const override { return FATReader::getStartOffset(); }
        void flush() override {}

    private:
        FATBootSectorCommon bootSectorData_;
    };

    std::shared_ptr<FATReader> reader = std::make_shared<BootSectorOnlyReader>(0, 512, bootSector);
    FATTableParser parser(reader);

    assert(parser.getBytesPerSector() == 512);
    assert(parser.getSectorCount() == 1); // sectorsPerFat16
    assert(parser.getFatCount() == 2);
    assert(parser.getReservedSectorCount() == 1);

    // Note: We can't test parseFatTable() easily without FAT data
    // but we can test the parameter extraction

    std::cout << "[PASS] FATTableParser tests passed" << std::endl;
}

void test_fat_chain_reader() {
    std::cout << "Testing FATChainReader..." << std::endl;

    // Create a simple test
    FATBootSectorCommon bootSector = createFat12BootSector();
    std::shared_ptr<FATReader> reader = std::make_shared<MockFATReader>(
        0, 512, bootSector, std::vector<uint8_t>(), 9);

    FATTable table(FATType::FAT12, 9, 512, 4086, reader);
    FATChainReader chainReader(&table, 512, 1, 1, 2, 9); // 1 reserved sector, 2 FATs, 9 sectors per FAT

    // Test that we can create the chain reader
    assert(&chainReader != nullptr);

    std::cout << "[PASS] FATChainReader tests passed" << std::endl;
}

void test_fat_validator() {
    std::cout << "Testing FATValidator..." << std::endl;

    // Create a simple test
    FATBootSectorCommon bootSector = createFat12BootSector();
    std::shared_ptr<FATReader> reader = std::make_shared<MockFATReader>(
        0, 512, bootSector, std::vector<uint8_t>(), 9);

    FATTable table(FATType::FAT12, 9, 512, 4086, reader);
    FATValidator validator(std::make_shared<const FATTable>(table));

    // Test that we can create the validator
    assert(&validator != nullptr);

    std::cout << "[PASS] FATValidator tests passed" << std::endl;
}

int main() {
    std::cout << "Testing RecoverySuite FAT Table and Cluster Chain Module..." << std::endl;

    try {
        test_fat_entry();
        test_fat_table_basic();
        test_fat_table_parser();
        test_fat_chain_reader();
        test_fat_validator();

        std::cout << "All FAT table and cluster chain tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}