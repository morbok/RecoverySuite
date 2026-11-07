#include <limits>
#include <iostream>
#include <memory>
#include <vector>
#include <cassert>
#include <cstring>
#include <map>
#include <string>

#include "Recovery/FilesystemDetector.h"
#include "Recovery/BootSectorDetection.h"
#include "Recovery/RecoveryCapability.h"
#include "Recovery/RecoverySafetyPolicy.h"
#include "Disk/IDiskReader.hpp"

// Mock disk reader for testing - returns specific boot sector data
class TestMockDiskReader : public recoverysuite::disk::IDiskReader {
public:
    TestMockDiskReader(const std::vector<std::byte>& bootSectorData = {},
                       uint32_t sectorSize = 512,
                       uint64_t totalSectors = 1000)
        : isOpen_(false),
          bootSectorData_(bootSectorData),
          sectorSize_(sectorSize),
          totalSectors_(totalSectors) {
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

        if (sectorCount == 0 || startSector >= totalSectors_ || sectorCount > totalSectors_ - startSector) {
            // For simplicity, only sector 0 (boot sector) is implemented
            return false;
        }

        // Calculate buffer size needed
        size_t bufferSizeNeeded = static_cast<size_t>(sectorCount) * static_cast<size_t>(sectorSize_);
        if (bufferSizeNeeded == 0) {
            return false;
        }

        buffer.resize(bufferSizeNeeded);

        // Copy boot sector data or fill with zeros
        if (!bootSectorData_.empty()) {
            size_t copySize = std::min(bootSectorData_.size(), bufferSizeNeeded);
            std::copy(bootSectorData_.begin(), bootSectorData_.begin() + copySize, buffer.begin());
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
        info.bytesPerSector = sectorSize_;
        info.totalSectors = totalSectors_;
        return info;
    }

    uint32_t getSectorSize() const noexcept override {
        return sectorSize_;
    }

    uint64_t getTotalSectors() const noexcept override {
        return totalSectors_;
    }

private:
    bool isOpen_;
    std::vector<std::byte> bootSectorData_;
    uint32_t sectorSize_;
    uint64_t totalSectors_;
};

// Helper to create FAT boot sector signature
std::vector<std::byte> createFatBootSector() {
    std::vector<std::byte> bootSector(512, std::byte{0});
    bootSector[11] = std::byte{0}; bootSector[12] = std::byte{2}; // 512 bytes/sector
    bootSector[13] = std::byte{1}; bootSector[14] = std::byte{1};
    bootSector[16] = std::byte{2}; bootSector[17] = std::byte{224};
    bootSector[19] = std::byte{0x40}; bootSector[20] = std::byte{0x0B}; // 2880 sectors
    bootSector[21] = std::byte{0xF0}; bootSector[22] = std::byte{9};
    bootSector[510] = std::byte{0x55}; bootSector[511] = std::byte{0xAA};
    return bootSector;
}

// Helper to create NTFS boot sector signature
std::vector<std::byte> createNtfsBootSector() {
    std::vector<std::byte> bootSector(512, std::byte{0});
    const std::string oem = "NTFS    ";
    std::memcpy(bootSector.data() + 3, oem.data(), oem.size());
    bootSector[11] = std::byte{0}; bootSector[12] = std::byte{2};
    bootSector[13] = std::byte{8};
    bootSector[40] = std::byte{0xE8}; bootSector[41] = std::byte{3};
    bootSector[48] = std::byte{4}; bootSector[64] = std::byte{0xF6};
    bootSector[510] = std::byte{0x55}; bootSector[511] = std::byte{0xAA};
    return bootSector;
}

// Helper to create empty boot sector
std::vector<std::byte> createEmptyBootSector() {
    return std::vector<std::byte>(512, std::byte{0});
}

// Test FilesystemDetector initialization
void test_filesystem_detector_initialization() {
    std::cout << "Testing FilesystemDetector initialization..." << std::endl;

    auto mockReader = std::make_shared<TestMockDiskReader>();
    mockReader->open("test");

    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_DETECTION);
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;

    // Test constructor with all parameters
    recoverysuite::recovery::FilesystemDetector detector(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    // Test constructor with default safety policy
    recoverysuite::recovery::FilesystemDetector detector2(
        capabilityRegistry,
        recoverysuite::recovery::RecoverySafetyPolicy(),
        mockReader.get()
    );

    // Test constructor with null diskReader (should work, just won't be able to read)
    recoverysuite::recovery::FilesystemDetector detector3(
        capabilityRegistry,
        safetyPolicy,
        nullptr
    );

    mockReader->close();
    std::cout << "[PASS] FilesystemDetector initialization test passed" << std::endl;
}

// Test FilesystemDetector detectFilesystems method with various inputs
void test_filesystem_detector_detection() {
    std::cout << "Testing FilesystemDetector detection..." << std::endl;

    auto mockReader = std::make_shared<TestMockDiskReader>();
    mockReader->open("test");

    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_DETECTION);
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;

    // Test 1: Empty boot sector (should detect nothing)
    mockReader = std::make_shared<TestMockDiskReader>(createEmptyBootSector());
    mockReader->open("test");
    recoverysuite::recovery::FilesystemDetector detector(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    std::vector<std::string> detectedFilesystems;
    bool result = detector.detectFilesystems(
        true,   // sourceIsReadOnly
        true,   // destinationDiffersFromSource
        0,      // startSector
        1,      // numSectors
        true,   // sourceRangesAreValid
        true,   // outputPathIsValid
        true,   // arithmeticCannotOverflow
        true,   // cancellationStateIsValid
        detectedFilesystems
    );

    assert(result == false); // Unknown data is not a detected filesystem.
    assert(detectedFilesystems.empty());  // Should detect nothing from empty data
    mockReader->close();

    // Test 2: FAT boot sector
    mockReader = std::make_shared<TestMockDiskReader>(createFatBootSector());
    mockReader->open("test");
    recoverysuite::recovery::FilesystemDetector detector2(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    std::vector<std::string> detectedFilesystems2;
    bool result2 = detector2.detectFilesystems(
        true,   // sourceIsReadOnly
        true,   // destinationDiffersFromSource
        0,      // startSector
        1,      // numSectors
        true,   // sourceRangesAreValid
        true,   // outputPathIsValid
        true,   // arithmeticCannotOverflow
        true,   // cancellationStateIsValid
        detectedFilesystems2
    );

    assert(result2 == true);
    assert(detectedFilesystems2 == std::vector<std::string>{"FAT12"});
    // Note: Actual detection depends on the implementation details of detectFilesystemType
    // For now we just verify it doesn't crash
    mockReader->close();

    // Test 3: NTFS boot sector
    mockReader = std::make_shared<TestMockDiskReader>(createNtfsBootSector());
    mockReader->open("test");
    recoverysuite::recovery::FilesystemDetector detector3(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    std::vector<std::string> detectedFilesystems3;
    bool result3 = detector3.detectFilesystems(
        true,   // sourceIsReadOnly
        true,   // destinationDiffersFromSource
        0,      // startSector
        1,      // numSectors
        true,   // sourceRangesAreValid
        true,   // outputPathIsValid
        true,   // arithmeticCannotOverflow
        true,   // cancellationStateIsValid
        detectedFilesystems3
    );

    assert(result3 == true);
    assert(detectedFilesystems3 == std::vector<std::string>{"NTFS"});
    mockReader->close();

    std::cout << "[PASS] FilesystemDetector detection test passed" << std::endl;
}

// Test sector range validation
void test_sector_range_validation() {
    std::cout << "Testing sector range validation..." << std::endl;

    auto mockReader = std::make_shared<TestMockDiskReader>(createNtfsBootSector());
    mockReader->open("test");

    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_DETECTION);
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;
    recoverysuite::recovery::FilesystemDetector detector(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    // Exercise range validation through the public operation, with read-only input.
    auto detectRange = [&detector](uint64_t start, uint64_t count) {
        std::vector<std::string> found;
        return detector.detectFilesystems(true, true, start, count, true, true, true, true, found);
    };
    // Test valid ranges
    assert(detectRange(0, 100) == true);
    assert(detectRange(50, 50) == true);
    assert(detectRange(900, 100) == true);  // Ends at 999

    // Test invalid ranges
    assert(detectRange(0, 0) == false);  // Zero length
    assert(detectRange(1000, 1) == false);  // Beyond disk (1000+1 > 1000)
    assert(detectRange(900, 200) == false);  // Extends beyond disk (900+200 > 1000)

    // Test overflow protection
    assert(detectRange(std::numeric_limits<uint64_t>::max(), 1) == false);
    assert(detectRange(std::numeric_limits<uint64_t>::max() - 100, 200) == false);

    mockReader->close();
    std::cout << "[PASS] Sector range validation test passed" << std::endl;
}

void test_malformed_boot_sectors() {
    auto bytes = createFatBootSector();
    std::vector<uint8_t> boot(bytes.size());
    std::memcpy(boot.data(), bytes.data(), bytes.size());
    assert(recoverysuite::recovery::detectBootSectorType(boot) == "FAT12");
    boot[13] = 0; // No divide by zero when sectors per cluster is corrupt.
    assert(recoverysuite::recovery::detectBootSectorType(boot).empty());
    boot[13] = 1;
    boot[19] = 1; boot[20] = 0; // Filesystem overhead exceeds total sectors.
    assert(recoverysuite::recovery::detectBootSectorType(boot).empty());
    boot.resize(31); // Truncated boot sector.
    assert(recoverysuite::recovery::detectBootSectorType(boot).empty());
}

int main() {
    std::cout << "Testing FilesystemDetector and related components..." << std::endl;

    try {
        test_filesystem_detector_initialization();
        test_filesystem_detector_detection();
        test_sector_range_validation();
        test_malformed_boot_sectors();

        std::cout << "All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}