#include <iostream>
#include <memory>
#include <vector>
#include <cassert>
#include <cstring>
#include "../../src/Application/Service/RecoveryService.h"
#include "../../src/Disk/IDiskReader.hpp"
#include "../../src/Recovery/RecoveryCapability.h"
#include "../../src/Recovery/RecoverySafetyPolicy.h"

// Mock disk reader for testing - simplified version that returns test data
class TestMockDiskReader : public recoverysuite::disk::IDiskReader {
public:
    TestMockDiskReader(const std::vector<std::byte>& dataToReturn = {}, uint64_t totalSectors = 1000)
        : isOpen_(false), dataToReturn_(dataToReturn), totalSectors_(totalSectors) {
        // Sector size is 512 bytes
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

        // Calculate sector size (512 bytes for our mock)
        const uint32_t sectorSize = 512;
        size_t bufferSizeNeeded = static_cast<size_t>(sectorCount) * static_cast<size_t>(sectorSize);

        if (bufferSizeNeeded == 0) {
            return false;
        }

        buffer.resize(bufferSizeNeeded);

        // For our test, we'll return test data if we have any, otherwise zeros
        if (!dataToReturn_.empty()) {
            size_t copySize = std::min(dataToReturn_.size(), bufferSizeNeeded);
            std::copy(dataToReturn_.begin(), dataToReturn_.begin() + copySize, buffer.begin());
            if (copySize < bufferSizeNeeded) {
                std::fill(buffer.begin() + copySize, buffer.end(), std::byte{0});
            }
        } else {
            std::fill(buffer.begin(), buffer.end(), std::byte{0});
        }
        return true;
    }

    bool writeSectors(uint64_t startSector, uint64_t sectorCount, const std::vector<std::byte>& buffer) override {
        // Mock implementation: pretend to write successfully
        if (!isOpen_) {
            throw recoverysuite::disk::DiskException("Disk is not open");
        }
        // For simplicity, we'll assume the mock is writable (ignore read-only)
        // Just return true to indicate success.
        return true;
    }

    recoverysuite::disk::DeviceInformation getDiskInfo() const override {
        recoverysuite::disk::DeviceInformation info;
        info.modelNumber = "test_mock";
        info.bytesPerSector = 512;
        info.totalSectors = totalSectors_;
        return info;
    }

    uint32_t getSectorSize() const noexcept override {
        return 512;
    }

    uint64_t getTotalSectors() const noexcept override {
        return totalSectors_;
    }

private:
    bool isOpen_;
    std::vector<std::byte> dataToReturn_;
    uint64_t totalSectors_;
};

// Test basic service initialization
void test_service_initialization() {
    std::cout << "Testing service initialization..." << std::endl;

    auto mockReader = std::make_shared<TestMockDiskReader>();
    mockReader->open("test");

    // Create service with default safety policy
    recoverysuite::application::service::RecoveryService service(mockReader);

    // Test that we can get disk info
    auto diskInfo = service.getDiskInfo();
    assert(diskInfo.bytesPerSector == 512);
    assert(diskInfo.totalSectors == 1000);
    assert(diskInfo.modelNumber == "test_mock");

    std::cout << "[PASS] Service initialization test passed" << std::endl;
    mockReader->close();
}

// Test sector validation
void test_sector_validation() {
    std::cout << "Testing sector validation..." << std::endl;

    auto mockReader = std::make_shared<TestMockDiskReader>();
    mockReader->open("test");

    recoverysuite::application::service::RecoveryService service(mockReader);

    // Test valid range
    assert(service.isSectorRangeValid(0, 10) == true);
    assert(service.isSectorRangeValid(100, 50) == true);

    // Test invalid ranges
    assert(service.isSectorRangeValid(0, 0) == false);  // Zero length
    assert(service.isSectorRangeValid(1000, 1) == false);  // Beyond disk
    assert(service.isSectorRangeValid(900, 200) == false);  // Extends beyond disk

    // Test overflow protection
    assert(service.isSectorRangeValid(std::numeric_limits<uint64_t>::max(), 1) == false);

    std::cout << "[PASS] Sector validation test passed" << std::endl;
    mockReader->close();
}

// Test filesystem analysis (should fail gracefully with empty data)
void test_filesystem_analysis() {
    std::cout << "Testing filesystem analysis..." << std::endl;

    // Create empty data
    std::vector<std::byte> emptyData;
    auto mockReader = std::make_shared<TestMockDiskReader>(emptyData);
    mockReader->open("test");

    recoverysuite::application::service::RecoveryService service(mockReader);

    // Analyze empty disk - should fail gracefully
    auto result = service.analyzeFilesystem(0, 1);

    // With empty data, analysis should fail but not crash
    std::cout << "[PASS] Filesystem analysis test completed (success: "
              << (result.success ? "true" : "false") << ", error: \"" << result.errorMessage << "\")" << std::endl;

    mockReader->close();
}

// Test file recovery (should fail gracefully with empty data)
void test_file_recovery() {
    std::cout << "Testing file recovery..." << std::endl;

    // Create empty data
    std::vector<std::byte> emptyData;
    auto mockReader = std::make_shared<TestMockDiskReader>(emptyData);
    mockReader->open("test");

    recoverysuite::application::service::RecoveryService service(mockReader);

    // Try to recover files from empty disk - should fail gracefully
    auto result = service.recoverFiles(0, 1);

    // With empty data, recovery should fail but not crash
    std::cout << "[PASS] File recovery test completed (success: "
              << (result.success ? "true" : "false") << ", error: \"" << result.errorMessage << "\")" << std::endl;

    mockReader->close();
}

// Test metadata recovery (should fail gracefully with empty data)
void test_metadata_recovery() {
    std::cout << "Testing metadata recovery..." << std::endl;

    // Create empty data
    std::vector<std::byte> emptyData;
    auto mockReader = std::make_shared<TestMockDiskReader>(emptyData);
    mockReader->open("test");

    recoverysuite::application::service::RecoveryService service(mockReader);

    // Try to recover metadata from empty disk - should fail gracefully
    auto result = service.recoverMetadata(0, 1);

    // With empty data, metadata recovery should fail but not crash
    std::cout << "[PASS] Metadata recovery test completed (success: "
              << (result.success ? "true" : "false") << ", error: \"" << result.errorMessage << "\")" << std::endl;

    mockReader->close();
}

// Test carving (should fail gracefully with empty data)
void test_carving() {
    std::cout << "Testing carving..." << std::endl;

    // Create empty data
    std::vector<std::byte> emptyData;
    auto mockReader = std::make_shared<TestMockDiskReader>(emptyData);
    mockReader->open("test");

    recoverysuite::application::service::RecoveryService service(mockReader);

    // Try to carve files from empty disk - should fail gracefully
    auto result = service.carveFiles(0, 1);

    // With empty data, carving should fail but not crash
    std::cout << "[PASS] Carving test completed (success: "
              << (result.success ? "true" : "false") << ", error: \"" << result.errorMessage << "\")" << std::endl;

    mockReader->close();
}

int main() {
    std::cout << "Testing RecoverySuite Service Layer..." << std::endl;

    try {
        test_service_initialization();
        test_sector_validation();
        test_filesystem_analysis();
        test_file_recovery();
        test_metadata_recovery();
        test_carving();

        std::cout << "All recovery service tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}