#include <RecoverySuite/Partition/PartitionManager.hpp>
#include <RecoverySuite/Partition/MBParser.hpp>
#include <RecoverySuite/Partition/PartitionValidator.hpp>
#include <RecoverySuite/Partition/PartitionException.hpp>
#include <RecoverySuite/Disk/IDiskReader.hpp>
#include <RecoverySuite/Disk/DiskException.hpp>
#include <vector>
#include <cstdint>
#include <iostream>
#include <cassert>
#include <memory>
#include <stdexcept>

class MockDiskReader : public RecoverySuite::Disk::IDiskReader {
public:
    MockDiskReader(const std::vector<std::byte>& sectorData) : sectorData_(sectorData) {}

    void open(const std::string& devicePath) override {
        // For the mock, we ignore the devicePath and just mark as open.
        isOpen_ = true;
    }

    void close() override {
        isOpen_ = false;
    }

    bool isOpen() const noexcept override {
        return isOpen_;
    }

    uint64_t readSectors(uint64_t startSector, uint64_t sectorCount, std::vector<std::byte>& buffer) override {
        if (!isOpen_) {
            throw RecoverySuite::Disk::DiskAccessException("Disk is not open");
        }
        if (startSector != 0) {
            throw RecoverySuite::Disk::DiskAccessException("Mock only supports reading sector 0");
        }
        if (sectorCount > 1) {
            throw RecoverySuite::Disk::DiskAccessException("Mock only supports reading 1 sector");
        }
        if (sectorData_.empty()) {
            return 0;
        }
        buffer = sectorData_;
        return 1;
    }

    RecoverySuite::Disk::DiskInfo getDiskInfo() const override {
        // Return a dummy DiskInfo
        RecoverySuite::Disk::DiskInfo info;
        info.devicePath = "mock";
        info.sectorSize = 512;
        // We don't set geometry, but the PartitionManager doesn't need it for parsing the MBR.
        return info;
    }

    uint32_t getSectorSize() const noexcept override {
        return 512;
    }

    uint64_t getTotalSectors() const noexcept override {
        return 0; // Not used in MBR parsing
    }

private:
    std::vector<std::byte> sectorData_;
    bool isOpen_ = false;
};

int main() {
    try {
        // Create a mock MBR with one partition (type 0x07, start LBA 2048, sector count 100000, bootable)
        std::vector<std::byte> mbrBuffer(512, std::byte{0}); // Initialize to zero

        // Set the boot signature (little-endian: 0x55AA -> AA 55)
        mbrBuffer[510] = std::byte{0xAA};
        mbrBuffer[511] = std::byte{0x55};

        // First partition entry (at offset 446)
        // Boot indicator: 0x80 (active)
        mbrBuffer[446] = std::byte{0x80};
        // Start CHS: we'll set to 0,0,2 for simplicity (but note: CHS is limited)
        mbrBuffer[447] = std::byte{0x00}; // head
        mbrBuffer[448] = std::byte{0x02}; // sector (bits 0-5) and cylinder high bits
        mbrBuffer[449] = std::byte{0x00}; // cylinder low 8 bits
        // Partition type: 0x07 (NTFS/HPFS)
        mbrBuffer[450] = std::byte{0x07};
        // End CHS: we'll set to 0,0,0
        mbrBuffer[451] = std::byte{0x00}; // head
        mbrBuffer[452] = std::byte{0x00}; // sector
        mbrBuffer[453] = std::byte{0x00}; // cylinder
        // Start LBA: 2048 (little endian)
        mbrBuffer[454] = std::byte{0x00 & 0xFF};
        mbrBuffer[455] = std::byte{(2048 >> 8) & 0xFF};
        mbrBuffer[456] = std::byte{(2048 >> 16) & 0xFF};
        mbrBuffer[457] = std::byte{(2048 >> 24) & 0xFF};
        // Sector count: 100000 (little endian)
        mbrBuffer[458] = std::byte{100000 & 0xFF};
        mbrBuffer[459] = std::byte{(100000 >> 8) & 0xFF};
        mbrBuffer[460] = std::byte{(100000 >> 16) & 0xFF};
        mbrBuffer[461] = std::byte{(100000 >> 24) & 0xFF};

        // The rest of the partition entries (entries 2,3,4) are zero.

        // Create the mock disk reader
        auto mockReader = std::make_shared<MockDiskReader>(mbrBuffer);
        mockReader->open("mock");

        // Test the MBRParser
        RecoverySuite::Partition::MBRParser parser(mockReader);
        auto [mbrHeader, partitionTable] = parser.parseMBR();

        // Check the MBR signature
        assert(mbrHeader.hasValidSignature());

        // Check the partition table
        assert(partitionTable.hasValidSignature());
        assert(partitionTable.getPartitionCount() == 1);

        // Get the first partition
        const auto& entry = partitionTable.getEntry(0);
        assert(!entry.isEmpty());
        assert(entry.getBootIndicator() == 0x80); // Bootable
        assert(entry.getPartitionType() == 0x07); // NTFS
        assert(entry.getStartLBA() == 2048);
        assert(entry.getSectorCount() == 100000);

        // Test the PartitionValidator
        RecoverySuite::Partition::PartitionValidator validator;
        validator.validate(partitionTable); // Should not throw

        // Test the PartitionManager
        RecoverySuite::Partition::PartitionManager manager(mockReader);
        auto [mbrHeader2, partitionTable2] = manager.readMBR();
        assert(mbrHeader2.hasValidSignature());
        assert(partitionTable2.hasValidSignature());

        // Validate partitions via manager
        manager.validatePartitions(); // Should not throw

        // Get partition geometries
        auto geometries = manager.getPartitionGeometries();
        assert(geometries.size() == 1);
        assert(geometries[0].getStartLBA() == 2048);
        assert(geometries[0].getSectorCount() == 100000);
        assert(geometries[0].getPartitionType() == 0x07);
        assert(geometries[0].isBootable() == true);

        std::cout << "All Partition module tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}