#include <iostream>
#include <memory>
#include <vector>
#include <cassert>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <map>
#include <string>

#include "../../src/Application/Service/RecoveryService.h"
#include "../../src/Disk/IDiskReader.hpp"
#include "../../src/Recovery/RecoveryCapability.h"
#include "../../src/Recovery/RecoverySafetyPolicy.h"
#include "../../src/Logging/Logger.h"

// Mock disk reader for testing - returns specific test data for different scenarios
class ApplicationLayerTestMockDiskReader : public recoverysuite::disk::IDiskReader {
public:
    ApplicationLayerTestMockDiskReader(const std::vector<std::byte>& dataToReturn = {},
                                       uint32_t sectorSize = 512,
                                       uint64_t totalSectors = 1000)
        : isOpen_(false),
          dataToReturn_(dataToReturn),
          totalSectors_(totalSectors),
          sectorSize_(sectorSize),
          readCalls_(0),
          shouldFailRead_(false) {
    }

    void open(const std::string& devicePath) override {
        (void)devicePath;
        isOpen_ = true;
        readCalls_ = 0;
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

        readCalls_++;

        // Simulate read failure if requested
        if (shouldFailRead_) {
            throw recoverysuite::disk::DiskException("Simulated read failure");
        }

        // Sector size is configurable
        size_t bufferSizeNeeded = static_cast<size_t>(sectorCount) * static_cast<size_t>(sectorSize_);

        if (bufferSizeNeeded == 0) {
            return false;
        }

        buffer.resize(bufferSizeNeeded);

        // Return test data if we have any, otherwise zeros
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
        info.modelNumber = "test_mock_app_layer";
        info.serialNumber = "APP123";
        info.firmwareRevision = "1.0";
        info.bytesPerSector = sectorSize_;
        info.totalSectors = totalSectors_;
        info.isRemovable = false;
        return info;
    }

    uint32_t getSectorSize() const noexcept override {
        return sectorSize_;
    }

    uint64_t getTotalSectors() const noexcept override {
        return totalSectors_;
    }

    // Test control methods
    void setShouldFailRead(bool fail) { shouldFailRead_ = fail; }
    size_t getReadCallCount() const { return readCalls_; }
    void setDataToReturn(const std::vector<std::byte>& data) { dataToReturn_ = data; }

private:
    bool isOpen_;
    std::vector<std::byte> dataToReturn_;
    uint64_t totalSectors_;
    uint32_t sectorSize_;
    size_t readCalls_;
    bool shouldFailRead_;
};

// Test application service layer initialization
void test_service_layer_initialization() {
    std::cout << "Testing service layer initialization..." << std::endl;

    auto mockReader = std::make_shared<ApplicationLayerTestMockDiskReader>();
    mockReader->open("test");

    // Create service with default safety policy
    recoverysuite::application::service::RecoveryService service(mockReader);

    // Test that we can get disk info
    auto diskInfo = service.getDiskInfo();
    assert(diskInfo.bytesPerSector == 512);
    assert(diskInfo.totalSectors == 1000);
    assert(diskInfo.modelNumber == "test_mock_app_layer");

    // Test sector validation
    assert(service.isSectorRangeValid(0, 10) == true);
    assert(service.isSectorRangeValid(100, 50) == true);
    assert(service.isSectorRangeValid(0, 0) == false);  // Zero length
    assert(service.isSectorRangeValid(1000, 1) == false);  // Beyond disk

    mockReader->close();
    std::cout << "[PASS] Service layer initialization test passed" << std::endl;
}

// Test filesystem analysis through service layer
void test_service_layer_filesystem_analysis() {
    std::cout << "Testing service layer filesystem analysis..." << std::endl;

    // Create minimal valid boot sector data for testing
    std::vector<std::byte> bootSectorData(512, std::byte{0});
    // Add some basic signature to make it recognizable
    bootSectorData[0x52] = std::byte{0x29};  // Extended boot signature

    auto mockReader = std::make_shared<ApplicationLayerTestMockDiskReader>(bootSectorData);
    mockReader->open("test");

    recoverysuite::application::service::RecoveryService service(mockReader);

    // Perform analysis
    auto result = service.analyzeFilesystem(0, 1);

    // With our test data, analysis should succeed or fail gracefully (not crash)
    std::cout << "[INFO] Filesystem analysis result: success="
              << (result.success ? "true" : "false")
              << ", error=\"" << result.errorMessage << "\"" << std::endl;

    // Even if analysis fails due to incomplete data, it should not crash
    // and should return a proper result object
    assert(!result.errorMessage.empty() || result.success); // Either success or has error message

    mockReader->close();
    std::cout << "[PASS] Service layer filesystem analysis test passed" << std::endl;
}

// Test file recovery through service layer
void test_service_layer_file_recovery() {
    std::cout << "Testing service layer file recovery..." << std::endl;

    // Create test data that looks like it could contain file structures
    std::vector<std::byte> testData(512 * 10, std::byte{0}); // 10 sectors of test data
    // Add some patterns that might be interpreted as file data
    for (size_t i = 0; i < testData.size(); i += 100) {
        testData[i] = std::byte{0xFF};  // Mark some bytes
    }

    auto mockReader = std::make_shared<ApplicationLayerTestMockDiskReader>(testData);
    mockReader->open("test");

    recoverysuite::application::service::RecoveryService service(mockReader);

    // Try to recover files
    auto result = service.recoverFiles(0, 10);

    // Should either succeed or fail gracefully
    std::cout << "[INFO] File recovery result: success="
              << (result.success ? "true" : "false")
              << ", error=\"" << result.errorMessage << "\""
              << ", recovered files: " << result.recoveredFiles.size() << std::endl;

    // Verify the result object is properly formed
    assert(!result.errorMessage.empty() || result.success); // Either success or has error message

    mockReader->close();
    std::cout << "[PASS] Service layer file recovery test passed" << std::endl;
}

// Test metadata recovery through service layer
void test_service_layer_metadata_recovery() {
    std::cout << "Testing service layer metadata recovery..." << std::endl;

    // Create test metadata-like data
    std::vector<std::byte> testData(512 * 5, std::byte{0}); // 5 sectors
    // Add some patterns that might be interpreted as metadata
    testData[0] = std::byte{0xEB};  // Boot sector jump instruction
    testData[2] = std::byte{0x90};  // NOP

    auto mockReader = std::make_shared<ApplicationLayerTestMockDiskReader>(testData);
    mockReader->open("test");

    recoverysuite::application::service::RecoveryService service(mockReader);

    // Try to recover metadata
    auto result = service.recoverMetadata(0, 5);

    // Should either succeed or fail gracefully
    std::cout << "[INFO] Metadata recovery result: success="
              << (result.success ? "true" : "false")
              << ", error=\"" << result.errorMessage << "\""
              << ", recovered metadata: " << result.recoveredMetadata.size() << std::endl;

    // Verify the result object is properly formed
    assert(!result.errorMessage.empty() || result.success); // Either success or has error message

    mockReader->close();
    std::cout << "[PASS] Service layer metadata recovery test passed" << std::endl;
}

// Test carving through service layer
void test_service_layer_carving() {
    std::cout << "Testing service layer carving..." << std::endl;

    // Create test data with some known file signatures for carving
    std::vector<std::byte> testData(512 * 20, std::byte{0}); // 20 sectors
    // Add JPEG signature
    testData[0] = std::byte{0xFF};
    testData[1] = std::byte{0xD8};
    testData[2] = std::byte{0xFF};
    testData[3] = std::byte{0xE0};
    // Add PNG signature
    testData[512] = std::byte{0x89};
    testData[513] = std::byte{0x50};
    testData[514] = std::byte{0x4E};
    testData[515] = std::byte{0x47};
    testData[516] = std::byte{0x0D};
    testData[517] = std::byte{0x0A};
    testData[518] = std::byte{0x1A};
    testData[519] = std::byte{0x0A};

    auto mockReader = std::make_shared<ApplicationLayerTestMockDiskReader>(testData);
    mockReader->open("test");

    recoverysuite::application::service::RecoveryService service(mockReader);

    // Try to carve files
    auto result = service.carveFiles(0, 20);

    // Should either succeed or fail gracefully
    std::cout << "[INFO] Carving result: success="
              << (result.success ? "true" : "false")
              << ", error=\"" << result.errorMessage << "\""
              << ", carved files: " << result.carvedFiles.size() << std::endl;

    // Verify the result object is properly formed
    assert(!result.errorMessage.empty() || result.success); // Either success or has error message

    mockReader->close();
    std::cout << "[PASS] Service layer carving test passed" << std::endl;
}

// Test error handling in service layer
void test_service_layer_error_handling() {
    std::cout << "Testing service layer error handling..." << std::endl;

    auto mockReader = std::make_shared<ApplicationLayerTestMockDiskReader>();
    // Configure to fail reads
    mockReader->setShouldFailRead(true);
    mockReader->open("test");

    recoverysuite::application::service::RecoveryService service(mockReader);

    // Test analysis with failing disk
    auto analysisResult = service.analyzeFilesystem(0, 1);
    assert(!analysisResult.success);
    assert(!analysisResult.errorMessage.empty());
    std::cout << "[INFO] Analysis error handling: " << analysisResult.errorMessage << std::endl;

    // Test file recovery with failing disk
    auto fileResult = service.recoverFiles(0, 1);
    assert(!fileResult.success);
    assert(!fileResult.errorMessage.empty());
    std::cout << "[INFO] File recovery error handling: " << fileResult.errorMessage << std::endl;

    // Test metadata recovery with failing disk
    auto metadataResult = service.recoverMetadata(0, 1);
    assert(!metadataResult.success);
    assert(!metadataResult.errorMessage.empty());
    std::cout << "[INFO] Metadata recovery error handling: " << metadataResult.errorMessage << std::endl;

    // Test carving with failing disk
    auto carvingResult = service.carveFiles(0, 1);
    assert(!carvingResult.success);
    assert(!carvingResult.errorMessage.empty());
    std::cout << "[INFO] Carving error handling: " << carvingResult.errorMessage << std::endl;

    mockReader->close();
    std::cout << "[PASS] Service layer error handling test passed" << std::endl;
}

// Test sector validation through service layer
void test_service_layer_sector_validation() {
    std::cout << "Testing service layer sector validation..." << std::endl;

    auto mockReader = std::make_shared<ApplicationLayerTestMockDiskReader>();
    mockReader->open("test");

    recoverysuite::application::service::RecoveryService service(mockReader);

    // Test valid ranges
    assert(service.isSectorRangeValid(0, 100) == true);
    assert(service.isSectorRangeValid(50, 50) == true);
    assert(service.isSectorRangeValid(900, 100) == true);  // Ends at 999 for 1000 sector disk

    // Test invalid ranges
    assert(service.isSectorRangeValid(0, 0) == false);  // Zero length
    assert(service.isSectorRangeValid(1000, 1) == false);  // Beyond disk (1000+1 > 1000)
    assert(service.isSectorRangeValid(900, 200) == false);  // Extends beyond disk (900+200 > 1000)

    // Test overflow protection
    assert(service.isSectorRangeValid(std::numeric_limits<uint64_t>::max(), 1) == false);
    assert(service.isSectorRangeValid(std::numeric_limits<uint64_t>::max() - 100, 200) == false);

    mockReader->close();
    std::cout << "[PASS] Service layer sector validation test passed" << std::endl;
}

// Test logging integration
void test_service_layer_logging() {
    std::cout << "Testing service layer logging integration..." << std::endl;

    auto mockReader = std::make_shared<ApplicationLayerTestMockDiskReader>();
    mockReader->open("test");

    recoverysuite::application::service::RecoveryService service(mockReader);

    // Perform an operation that should generate some log output
    auto result = service.analyzeFilesystem(0, 1);

    // We can't easily capture log output in this test, but we can verify
    // the service doesn't crash when logging is used
    std::cout << "[INFO] Service layer logging test completed (no crash)" << std::endl;

    mockReader->close();
    std::cout << "[PASS] Service layer logging integration test passed" << std::endl;
}

// Test recovery capability enum usage
void test_recovery_capability_usage() {
    std::cout << "Testing recovery capability usage..." << std::endl;

    // Test that all expected capabilities exist
    assert(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS !=
           recoverysuite::recovery::RecoveryCapability::NONE);
    assert(recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY !=
           recoverysuite::recovery::RecoveryCapability::NONE);
    assert(recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY !=
           recoverysuite::recovery::RecoveryCapability::NONE);
    assert(recoverysuite::recovery::RecoveryCapability::CARVING !=
           recoverysuite::recovery::RecoveryCapability::NONE);
    assert(recoverysuite::recovery::RecoveryCapability::OUTPUT_EXPORT !=
           recoverysuite::recovery::RecoveryCapability::NONE);

    // Test that NONE is actually zero
    assert(static_cast<uint32_t>(recoverysuite::recovery::RecoveryCapability::NONE) == 0);

    std::cout << "[PASS] Recovery capability usage test passed" << std::endl;
}

int main() {
    std::cout << "Testing RecoverySuite Application Layer Readiness..." << std::endl;

    try {
        // Initialize logger
        recoverysuite::logging::Logger::instance().setLogLevel(recoverysuite::logging::LogLevel::DEBUG);

        test_service_layer_initialization();
        test_service_layer_filesystem_analysis();
        test_service_layer_file_recovery();
        test_service_layer_metadata_recovery();
        test_service_layer_carving();
        test_service_layer_error_handling();
        test_service_layer_sector_validation();
        test_service_layer_logging();
        test_recovery_capability_usage();

        std::cout << "All application layer readiness tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}