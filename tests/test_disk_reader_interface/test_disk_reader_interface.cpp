#include <iostream>
#include <memory>
#include <vector>
#include <cassert>
#include <cstring>
#include <stdexcept>

#include "../../src/Disk/IDiskReader.hpp"

// Mock disk reader implementation for testing the IDiskReader interface
class TestDiskReader : public recoverysuite::disk::IDiskReader {
public:
    TestDiskReader(const std::vector<std::byte>& testData = {},
                   uint32_t sectorSize = 512,
                   uint64_t totalSectors = 1000)
        : isOpen_(false),
          testData_(testData),
          sectorSize_(sectorSize),
          totalSectors_(totalSectors),
          openCalled_(false),
          closeCalled_(false),
          readSectorsCalled_(0),
          writeSectorsCalled_(0),
          getDiskInfoCalled_(0),
          getSectorSizeCalled_(0),
          getTotalSectorsCalled_(0) {
    }

    void open(const std::string& devicePath) override {
        (void)devicePath; // unused in mock
        isOpen_ = true;
        openCalled_ = true;
        lastOpenedPath_ = devicePath;
    }

    void close() override {
        isOpen_ = false;
        closeCalled_ = true;
    }

    bool isOpen() const noexcept override {
        return isOpen_;
    }

    bool readSectors(uint64_t startSector, uint64_t sectorCount, std::vector<std::byte>& buffer) override {
        readSectorsCalled_++;
        lastReadStartSector_ = startSector;
        lastReadSectorCount_ = sectorCount;

        if (!isOpen_) {
            throw recoverysuite::disk::DiskException("Disk is not open");
        }

        // For simplicity, only allow reading from sector 0 in this test
        if (startSector != 0) {
            return false;
        }

        // Calculate buffer size needed
        size_t bufferSizeNeeded = static_cast<size_t>(sectorCount) * static_cast<size_t>(sectorSize_);
        if (bufferSizeNeeded == 0) {
            return false;
        }

        buffer.resize(bufferSizeNeeded);

        // Copy test data or fill with zeros
        if (!testData_.empty()) {
            size_t copySize = std::min(testData_.size(), bufferSizeNeeded);
            std::copy(testData_.begin(), testData_.begin() + copySize, buffer.begin());
            if (copySize < bufferSizeNeeded) {
                std::fill(buffer.begin() + copySize, buffer.end(), std::byte{0});
            }
        } else {
            std::fill(buffer.begin(), buffer.end(), std::byte{0});
        }
        return true;
    }

    bool writeSectors(uint64_t startSector, uint64_t sectorCount, const std::vector<std::byte>& buffer) override {
        writeSectorsCalled_++;
        lastWriteStartSector_ = startSector;
        lastWriteSectorCount_ = sectorCount;
        lastWriteBuffer_ = buffer; // Copy for inspection

        if (!isOpen_) {
            throw recoverysuite::disk::DiskException("Disk is not open");
        }
        // For simplicity, we'll assume the mock is writable (ignore read-only)
        // Just return true to indicate success.
        return true;
    }

    recoverysuite::disk::DeviceInformation getDiskInfo() const override {
        getDiskInfoCalled_++;
        recoverysuite::disk::DeviceInformation info;
        info.modelNumber = "test_mock";
        info.bytesPerSector = sectorSize_;
        info.totalSectors = totalSectors_;
        return info;
    }

    uint32_t getSectorSize() const noexcept override {
        getSectorSizeCalled_++;
        return sectorSize_;
    }

    uint64_t getTotalSectors() const noexcept override {
        getTotalSectorsCalled_++;
        return totalSectors_;
    }

    // Helper methods to check call counts
    bool wasOpenCalled() const { return openCalled_; }
    bool wasCloseCalled() const { return closeCalled_; }
    size_t getReadSectorsCallCount() const { return readSectorsCalled_; }
    size_t getWriteSectorsCallCount() const { return writeSectorsCalled_; }
    size_t getGetDiskInfoCallCount() const { return getDiskInfoCalled_; }
    size_t getGetSectorSizeCallCount() const { return getSectorSizeCalled_; }
    size_t getGetTotalSectorsCallCount() const { return getTotalSectorsCalled_; }

    // Helper methods to get last called parameters
    std::string getLastOpenedPath() const { return lastOpenedPath_; }
    uint64_t getLastReadStartSector() const { return lastReadStartSector_; }
    uint64_t getLastReadSectorCount() const { return lastReadSectorCount_; }
    uint64_t getLastWriteStartSector() const { return lastWriteStartSector_; }
    uint64_t getLastWriteSectorCount() const { return lastWriteSectorCount_; }
    const std::vector<std::byte>& getLastWriteBuffer() const { return lastWriteBuffer_; }

private:
    bool isOpen_;
    std::vector<std::byte> testData_;
    uint32_t sectorSize_;
    uint64_t totalSectors_;

    // Call tracking - made mutable to allow modification in const methods
    mutable bool openCalled_;
    mutable bool closeCalled_;
    mutable size_t readSectorsCalled_;
    mutable size_t writeSectorsCalled_;
    mutable size_t getDiskInfoCalled_;
    mutable size_t getSectorSizeCalled_;
    mutable size_t getTotalSectorsCalled_;

    // Last called parameters
    std::string lastOpenedPath_;
    uint64_t lastReadStartSector_;
    uint64_t lastReadSectorCount_;
    uint64_t lastWriteStartSector_;
    uint64_t lastWriteSectorCount_;
    std::vector<std::byte> lastWriteBuffer_;
};

// Test IDiskReader interface behavior
void test_idiskreader_interface() {
    std::cout << "Testing IDiskReader interface behavior..." << std::endl;

    // Test data: a simple pattern
    std::vector<std::byte> testData = {
        std::byte{0x00}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03},
        std::byte{0x04}, std::byte{0x05}, std::byte{0x06}, std::byte{0x07}
    };

    auto diskReader = std::make_shared<TestDiskReader>(testData, 512, 1000);

    // Test initial state
    assert(!diskReader->wasOpenCalled());
    assert(!diskReader->wasCloseCalled());
    assert(diskReader->getReadSectorsCallCount() == 0);
    assert(diskReader->getWriteSectorsCallCount() == 0);
    assert(diskReader->getGetDiskInfoCallCount() == 0);
    assert(diskReader->getGetSectorSizeCallCount() == 0);
    assert(diskReader->getGetTotalSectorsCallCount() == 0);
    assert(!diskReader->isOpen());

    // Test open
    diskReader->open("/dev/test");
    assert(diskReader->wasOpenCalled());
    assert(diskReader->getLastOpenedPath() == "/dev/test");
    assert(diskReader->isOpen());

    // Test getDiskInfo
    auto info = diskReader->getDiskInfo();
    assert(diskReader->getGetDiskInfoCallCount() == 1);
    assert(info.bytesPerSector == 512);
    assert(info.totalSectors == 1000);
    assert(info.modelNumber == "test_mock");

    // Test getSectorSize
    (void)diskReader->getSectorSize();
    assert(diskReader->getGetSectorSizeCallCount() == 1);
    assert(diskReader->getSectorSize() == 512);

    // Test getTotalSectors
    (void)diskReader->getTotalSectors();
    assert(diskReader->getGetTotalSectorsCallCount() == 1);
    assert(diskReader->getTotalSectors() == 1000);

    // Test readSectors
    std::vector<std::byte> readBuffer;
    (void)diskReader->readSectors(0, 1, readBuffer);
    assert(diskReader->getReadSectorsCallCount() == 1);
    assert(diskReader->getLastReadStartSector() == 0);
    assert(diskReader->getLastReadSectorCount() == 1);
    assert(readBuffer.size() == 512); // sector size
    // Check first 8 bytes match test data
    for (size_t i = 0; i < testData.size(); i++) {
        assert(readBuffer[i] == testData[i]);
    }
    // Check rest are zero
    for (size_t i = testData.size(); i < readBuffer.size(); i++) {
        assert(readBuffer[i] == std::byte{0});
    }

    // Test readSectors with multiple sectors
    std::vector<std::byte> readBuffer2;
    (void)diskReader->readSectors(0, 2, readBuffer2);
    assert(diskReader->getReadSectorsCallCount() == 2);
    assert(readBuffer2.size() == 1024); // 2 sectors
    // First sector should have test data, second should be zeros
    for (size_t i = 0; i < testData.size(); i++) {
        assert(readBuffer2[i] == testData[i]);
    }
    for (size_t i = testData.size(); i < readBuffer2.size(); i++) {
        assert(readBuffer2[i] == std::byte{0});
    }

    // Test writeSectors
    std::vector<std::byte> writeData(512, std::byte{0xFF});
    (void)diskReader->writeSectors(0, 1, writeData);
    assert(diskReader->getWriteSectorsCallCount() == 1);
    assert(diskReader->getLastWriteStartSector() == 0);
    assert(diskReader->getLastWriteSectorCount() == 1);
    assert(diskReader->getLastWriteBuffer().size() == 512);
    for (size_t i = 0; i < writeData.size(); i++) {
        assert(diskReader->getLastWriteBuffer()[i] == writeData[i]);
    }

    // Test close
    diskReader->close();
    assert(diskReader->wasCloseCalled());
    assert(!diskReader->isOpen());

    // Test operations on closed disk should fail
    try {
        diskReader->open("/dev/test2");
        // Note: Our mock allows reopening, so this won't throw
    } catch (const std::exception&) {
        // Expected - though our mock doesn't actually throw on reopen
    }

    try {
        diskReader->getDiskInfo();
        assert(false); // Should not reach here
    } catch (const recoverysuite::disk::DiskException&) {
        // Expected
    }

    try {
        std::vector<std::byte> buffer;
        diskReader->readSectors(0, 1, buffer);
        assert(false); // Should not reach here
    } catch (const recoverysuite::disk::DiskException&) {
        // Expected
    }

    try {
        std::vector<std::byte> buffer(512, std::byte{0});
        diskReader->writeSectors(0, 1, buffer);
        assert(false); // Should not reach here
    } catch (const recoverysuite::disk::DiskException&) {
        // Expected
    }

    std::cout << "[PASS] IDiskReader interface test passed" << std::endl;
}

// Test IDiskReader with empty data
void test_idiskreader_empty_data() {
    std::cout << "Testing IDiskReader with empty data..." << std::endl;

    auto diskReader = std::make_shared<TestDiskReader>();

    diskReader->open("/dev/test");

    // Test readSectors with empty test data
    std::vector<std::byte> readBuffer;
    bool readResult = diskReader->readSectors(0, 1, readBuffer);
    assert(readResult == true);
    assert(readBuffer.size() == 512);
    // All should be zero
    for (size_t i = 0; i < readBuffer.size(); i++) {
        assert(readBuffer[i] == std::byte{0});
    }

    diskReader->close();
    std::cout << "[PASS] IDiskReader empty data test passed" << std::endl;
}

// Test IDiskReader sector validation boundaries
void test_idiskreader_boundary_conditions() {
    std::cout << "Testing IDiskReader boundary conditions..." << std::endl;

    auto diskReader = std::make_shared<TestDiskReader>(std::vector<std::byte>{}, 512, 100);
    diskReader->open("/dev/test");

    // Test reading at boundary
    std::vector<std::byte> buffer;
    bool result = diskReader->readSectors(99, 1, buffer); // Last sector
    assert(result == true); // Should succeed

    // Test reading beyond boundary
    result = diskReader->readSectors(100, 1, buffer); // First sector beyond disk
    assert(result == false); // Should fail

    // Test reading that extends beyond boundary
    result = diskReader->readSectors(95, 10, buffer); // Goes from 95 to 104
    assert(result == false); // Should fail

    diskReader->close();
    std::cout << "[PASS] IDiskReader boundary conditions test passed" << std::endl;
}

int main() {
    std::cout << "Testing IDiskReader interface behavior..." << std::endl;

    try {
        test_idiskreader_interface();
        test_idiskreader_empty_data();
        test_idiskreader_boundary_conditions();

        std::cout << "All IDiskReader interface tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}