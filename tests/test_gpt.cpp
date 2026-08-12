#include <RecoverySuite/Partition/GPTParser.hpp>
#include <RecoverySuite/Partition/GPTHeader.hpp>
#include <RecoverySuite/Partition/GPTPartitionEntry.hpp>
#include <RecoverySuite/Partition/GPTPartitionTable.hpp>
#include <RecoverySuite/Partition/GPTValidator.hpp>
#include "IDiskReader.hpp"
#include "DiskExceptions.h"
#include <vector>
#include <cstdint>
#include <iostream>
#include <cassert>
#include <cstring>
#include <memory>
#include <stdexcept>

class MockDiskReader : public recoverysuite::disk::IDiskReader {
public:
    MockDiskReader(const std::vector<std::byte>& sector0Data,
                   const std::vector<std::byte>& sector1Data,
                   const std::vector<std::byte>& backupHeaderData,
                   const std::vector<std::byte>& entryArrayData,
                   uint64_t totalSectorsArg = 1000)
        : sector0Data_(sector0Data)
        , sector1Data_(sector1Data)
        , backupHeaderData_(backupHeaderData)
        , entryArrayData_(entryArrayData)
        , totalSectors_(totalSectorsArg)
        , isOpen_(false)
    {
    }

    void open(const std::string& devicePath) override {
        (void)devicePath;
        isOpen_ = true;
    }

    void close() override {
        isOpen_ = false;
    }

    bool isOpen() const noexcept override {
        return isOpen_;
    }

    bool readSectors(uint64_t startSector, uint64_t sectorCount, std::vector<std::byte>& buffer) override {
        if (!isOpen_) {
            throw recoverysuite::disk::DiskException("Disk is not open");
        }
        // We only support reading specific sectors for simplicity in this test.
        // We'll handle reading sector 0, 1, backup LBA, and entry array LBA.
        if (startSector == 0 && sectorCount == 1) {
            buffer = sector0Data_;
            return true;
        }
        if (startSector == 1 && sectorCount == 1) {
            buffer = sector1Data_;
            return true;
        }
        // For backup header, we assume it's at the last sector
        if (startSector == totalSectors_ - 1 && sectorCount == 1) {
            buffer = backupHeaderData_;
            return true;
        }
        // For entry array, we assume it's provided as a contiguous buffer that may span multiple sectors.
        // We'll calculate how many sectors we need based on the buffer size and sector size.
        // But for simplicity, we'll assume the entry array data is exactly the buffer we need and
        // the startSector matches the partition-entry LBA and sectorCount is enough to cover it.
        // We'll check if the startSector matches our fake entryArrayLBA and if the buffer size matches.
        // This is a simplified mock; in reality, we would need to know the entryArrayLBA and entry array size.
        // We'll set entryArrayLBA to 2 for simplicity in the test.
        if (startSector == 2 && sectorCount * 512 >= entryArrayData_.size()) {
            // We'll copy as much as we need
            if (buffer.size() < entryArrayData_.size()) {
                buffer.resize(entryArrayData_.size());
            }
            std::copy(entryArrayData_.begin(), entryArrayData_.end(), buffer.begin());
            return true; // Indicate successful read
        }
        // If we don't recognize the request, return false (or throw?)
        // For simplicity, we'll throw an exception to indicate unsupported read.
        throw recoverysuite::disk::DiskException("Unsupported sector read in mock");
    }

    bool writeSectors(uint64_t startSector, uint64_t sectorCount, const std::vector<std::byte>& buffer) override {
        // Mock implementation: pretend to write successfully
        // We don't actually write data, but we can check that the request is valid
        if (!isOpen_) {
            throw recoverysuite::disk::DiskException("Disk is not open");
        }
        // For simplicity, we'll assume the mock is writable (ignore read-only)
        // Just return true to indicate success.
        return true;
    }

    recoverysuite::disk::DeviceInformation getDiskInfo() const override {
        recoverysuite::disk::DeviceInformation info;
        info.modelNumber = "mock";
        info.bytesPerSector = 512;
        info.totalSectors = totalSectors_;
        // We don't set geometry, but the PartitionManager doesn't need it for parsing the GPT.
        return info;
    }

    uint32_t getSectorSize() const noexcept override {
        return 512;
    }

    uint64_t getTotalSectors() const noexcept override {
        return totalSectors_;
    }

private:
    std::vector<std::byte> sector0Data_;
    std::vector<std::byte> sector1Data_;
    std::vector<std::byte> backupHeaderData_;
    std::vector<std::byte> entryArrayData_;
    uint64_t totalSectors_;
    bool isOpen_;
};

int main() {
    try {
        // Create a mock GPT disk with one partition (type EF00, start LBA 2048, size 100000 sectors, name "Test")
        // We'll create a protective MBR in sector 0, a valid GPT header in sector 1, and partition entry array starting at sector 2.

        const uint32_t sectorSize = 512;

        // Sector 0: Protective MBR
        std::vector<std::byte> mbrBuffer(sectorSize, std::byte{0});
        // Set the boot signature
        mbrBuffer[510] = std::byte{0xAA};
        mbrBuffer[511] = std::byte{0x55};
        // Set one partition entry (the first one) as type 0xEE (protective MBR)
        // Partition table starts at offset 446
        mbrBuffer[446] = std::byte{0x80}; // boot indicator (active)
        // CHS fields we can ignore
        mbrBuffer[447] = std::byte{0x00};
        mbrBuffer[448] = std::byte{0x00};
        mbrBuffer[449] = std::byte{0x00};
        mbrBuffer[450] = std::byte{0xEE}; // partition type 0xEE
        // CHS for end
        mbrBuffer[451] = std::byte{0x00};
        mbrBuffer[452] = std::byte{0x00};
        mbrBuffer[453] = std::byte{0x00};
        // Starting LBA: we'll set to 1 (so the protective MBR covers from LBA 1 to end? Actually protective MBR usually covers the whole disk)
        // We'll set starting LBA to 1 (so it covers from LBA 1 to the end of disk)
        mbrBuffer[454] = std::byte{0x01};
        mbrBuffer[455] = std::byte{0x00};
        mbrBuffer[456] = std::byte{0x00};
        mbrBuffer[457] = std::byte{0x00};
        // Size in LBA: we'll set to totalSectors-1 (so it covers from LBA 1 to the last sector)
        // For simplicity, we'll set a small size.
        uint32_t size = 100; // 100 sectors
        mbrBuffer[458] = std::byte{static_cast<unsigned char>(size & 0xFF)};
        mbrBuffer[459] = std::byte{static_cast<unsigned char>((size >> 8) & 0xFF)};
        mbrBuffer[460] = std::byte{static_cast<unsigned char>((size >> 16) & 0xFF)};
        mbrBuffer[461] = std::byte{static_cast<unsigned char>((size >> 24) & 0xFF)};

        // Sector 1: GPT Header
        std::vector<std::byte> gptHeaderBuffer(sectorSize, std::byte{0});
        // We'll place the GPT header at the start of the sector
        recoverysuite::partition::GPTHeader* headerPtr = reinterpret_cast<recoverysuite::partition::GPTHeader*>(gptHeaderBuffer.data());
        // Set signature
        headerPtr->signature = {'E','F','I',' ','P','A','R','T'};
        std::cout << "Signature: ";
        for (char c : headerPtr->signature) {
            std::cout << std::hex << (int)(unsigned char)c << " ";
        }
        std::cout << std::dec << std::endl;
        // Set revision 1.0
        headerPtr->revision = 0x00010000;
        // Set header size (usually 92 bytes for revision 1.0)
        // The UEFI spec says the header is 92 bytes for revision 1.0
        headerPtr->headerSize = 92;
        std::cout << "headerPtr->headerSize: " << headerPtr->headerSize << std::endl;
        // We'll set headerCRC32 to 0 for now and compute later.
        headerPtr->headerCRC32 = 0;
        headerPtr->reserved = 0;
        // Set myLBA to 1 (primary header)
        headerPtr->myLBA = 1;
        // Set backup LBA to totalSectors-1 (we'll set totalSectors to 1000 later)
        headerPtr->alternateLBA = 999;
        // Set first usable LBA to 2048 (typical)
        headerPtr->firstUsableLBA = 2048;
        // Set last usable LBA to 2048+100000-1 = 102047
        headerPtr->lastUsableLBA = 102047;
        // Set disk GUID to something
        std::array<uint8_t,16> diskGuid = {{0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x00}};
        std::copy(diskGuid.begin(), diskGuid.end(), headerPtr->diskGUID.begin());
        // Set partition-entry LBA to 2 (we'll put the entry array starting at sector 2)
        headerPtr->partitionEntryLBA = 2;
        // Set number of partition entries to 1
        headerPtr->numPartitionEntries = 1;
        // Set size of each partition entry to 128 (typical)
        headerPtr->sizeofPartitionEntry = 128;
        // Set partition-entry-array CRC32 to 0 for now, we'll compute later.
        headerPtr->partitionEntryArrayCRC32 = 0;
        // Debug: print the partitionEntryArrayCRC32 field (offset 88-91)
        std::cout << "Test: partitionEntryArrayCRC32 field (offset 88-91): ";
        for (size_t i = 0; i < 4; ++i) {
            std::cout << std::hex << (int)(unsigned char)(((uint8_t*)&headerPtr->partitionEntryArrayCRC32)[i]) << " ";
        }
        std::cout << std::dec << std::endl;
        // Let's also set up the entry array data so we can compute its CRC
        recoverysuite::partition::GPTValidator validator;
        std::vector<std::byte> entryArrayBuffer(sectorSize, std::byte{0}); // we only need one sector for one entry
        recoverysuite::partition::GPTPartitionEntry* entryPtr = reinterpret_cast<recoverysuite::partition::GPTPartitionEntry*>(entryArrayBuffer.data());
        // Set partition type GUID to EF00 (EFI System Partition)
        std::array<uint8_t,16> efGuid = {{0x28,0x73,0x2A,0xC1,0x1F,0xF8,0xD2,0x11,0xBA,0x4B,0x00,0xA0,0xC9,0x3E,0xC9,0x3B}};
        std::copy(efGuid.begin(), efGuid.end(), entryPtr->getPartitionTypeGUID().begin());
        // Set unique partition GUID to something
        std::array<uint8_t,16> partGuid = {{0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10}};
        std::copy(partGuid.begin(), partGuid.end(), entryPtr->getUniquePartitionGUID().begin());
        // Set starting LBA to 2048
        uint64_t startLba = 2048;
        ::memcpy(entryArrayBuffer.data()+0x20, &startLba, 8);
        // Set ending LBA to 2048+100000-1 = 102047
        uint64_t endLba = 102047;
        ::memcpy(entryArrayBuffer.data()+0x28, &endLba, 8);
        // Set attributes to 0
        uint64_t attributes = 0;
        ::memcpy(entryArrayBuffer.data()+0x30, &attributes, 8);
        // Set partition name to "Test" in UTF-16
        std::u16string name = u"Test";
        // Copy the name into the entry (offset 0x38, up to 72 characters)
        ::memcpy(entryArrayBuffer.data()+0x38, name.c_str(), (name.size()+1)*sizeof(char16_t)); // include null terminator

        // Now compute the CRC32 of the entry array (we have only one sector, but the entry array size is 128 bytes)
        // We'll zero the entryArrayCRC32 in the header, compute the CRC of the entry array data (128 bytes), then set.
        // We'll create a buffer that is exactly the entry array data (128 bytes) from the entryArrayBuffer.
        std::vector<std::byte> entryArrayData(entryArrayBuffer.begin(), entryArrayBuffer.begin()+128);
        uint32_t entryArrayCrc = validator.computeCRC32(entryArrayData.data(), entryArrayData.size());
        headerPtr->partitionEntryArrayCRC32 = entryArrayCrc;
        std::cout << "Computed entry array CRC32: 0x" << std::hex << entryArrayCrc << std::dec << std::endl;
        // reserved2 is already zero.

        // Now compute the header CRC32 (we need to compute it with the headerCRC32 field set to 0)
        // We'll do it manually for the test, but we can also use the validator's compute function.
        // For simplicity, we'll set the headerCRC32 to the correct value by using the validator.
        // Print the address of the computeCRC32 function
        std::cout << "Validator test: computeCRC32 address: " << &recoverysuite::partition::GPTValidator::computeCRC32 << std::endl;
        // We need to compute the CRC of the header with the headerCRC32 field zeroed.
        // We'll copy the header buffer, zero the CRC field, compute, then set.
        std::vector<std::byte> headerCopy = gptHeaderBuffer;
        // Zero out the headerCRC32 field (offset 16, 4 bytes)
        headerCopy[16] = std::byte{0};
        headerCopy[17] = std::byte{0};
        headerCopy[18] = std::byte{0};
        headerCopy[19] = std::byte{0};
        // Print the header buffer (first 32 bytes) to see what we are hashing
        std::cout << "Header buffer (first 32 bytes): ";
        for (size_t i = 0; i < 32 && i < headerCopy.size(); ++i) {
            std::cout << std::hex << (int)(unsigned char)headerCopy[i] << " ";
        }
        std::cout << std::dec << std::endl;
        // Compute simple sum of all bytes for debugging
        uint64_t sum = 0;
        for (size_t i = 0; i < headerCopy.size(); ++i) {
            sum += static_cast<uint64_t>(headerCopy[i]);
        }
        std::cout << "Header buffer sum: " << sum << std::endl;
        // Print sums in 64-byte chunks
        for (size_t chunk = 0; chunk < headerCopy.size() / 64; ++chunk) {
            uint64_t chunkSum = 0;
            for (size_t i = 0; i < 64; ++i) {
                chunkSum += static_cast<uint64_t>(headerCopy[chunk * 64 + i]);
            }
            std::cout << "  Chunk " << chunk << " (offset " << chunk * 64 << "): " << chunkSum << std::endl;
        }
        if (headerCopy.size() % 64 != 0) {
            uint64_t chunkSum = 0;
            size_t start = (headerCopy.size() / 64) * 64;
            for (size_t i = start; i < headerCopy.size(); ++i) {
                chunkSum += static_cast<uint64_t>(headerCopy[i]);
            }
            std::cout << "  Last chunk (offset " << start << "): " << chunkSum << std::endl;
        }
        uint32_t headerCrc = validator.computeCRC32(headerCopy.data(), headerCopy.size());
        std::cout << "Computed header CRC32: 0x" << std::hex << headerCrc << std::dec << std::endl;


        // Let's also print out the header buffer to see what's in it
        std::cout << "Header buffer (first 32 bytes): ";
        for (size_t i = 0; i < 32 && i < headerCopy.size(); ++i) {
            std::cout << std::hex << (int)(unsigned char)headerCopy[i] << " ";
        }
        std::cout << std::dec << std::endl;

        headerPtr->headerCRC32 = headerCrc;

        // Print the header after setting the CRC (from gptHeaderBuffer)
        std::cout << "Header after setting CRC (first 32 bytes): ";
        for (size_t i = 0; i < 32 && i < gptHeaderBuffer.size(); ++i) {
            std::cout << std::hex << (int)(unsigned char)gptHeaderBuffer[i] << " ";
        }
        std::cout << std::dec << std::endl;


        // Now we have:
        // sector0: MBR with protective MBR
        // sector1: GPT header
        // sector2: partition entry array (one entry)

        // Create the mock disk reader
        auto mockReader = std::make_shared<MockDiskReader>(
            mbrBuffer,          // sector0
            gptHeaderBuffer,    // sector1
            std::vector<std::byte>(), // backup header (we won't use backup in this test, but we need to provide something)
            entryArrayBuffer,   // entry array data (we'll pass the whole sector, but the reader will use only the needed bytes)
            1000                // total sectors
        );
        mockReader->open("mock");

        // Parse the GPT
        recoverysuite::partition::GPTParser parser(mockReader);
        auto [gptHeader, gptTable] = parser.parseGPT();

        // Check the header
        assert(gptHeader.hasValidSignature());
        assert(gptHeader.myLBA == 1);
        assert(gptHeader.alternateLBA == 999);
        assert(gptHeader.firstUsableLBA == 2048);
        assert(gptHeader.lastUsableLBA == 102047);
        assert(gptHeader.numPartitionEntries == 1);
        assert(gptHeader.sizeofPartitionEntry == 128);

        // Check the partition table
        assert(gptTable.getPartitionCount() == 1);
        const auto& entry = gptTable.getEntry(0);
        assert(!entry.isEmpty());
        // Check partition type GUID (EF00)
        // We'll compare the first 4 bytes for simplicity
        auto typeGuid = entry.getPartitionTypeGUID();
        assert(typeGuid[0] == 0x28 && typeGuid[1] == 0x73 && typeGuid[2] == 0x2A && typeGuid[3] == 0xC1);
        // Check starting LBA
        assert(entry.getStartingLBA() == 2048);
        // Check ending LBA
        assert(entry.getEndingLBA() == 102047);
        // Check name
        std::u16string nameFromEntry = entry.getName();
        assert(nameFromEntry == u"Test");

        std::cout << "GPT test passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}