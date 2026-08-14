#include <iostream>
#include <memory>
#include <vector>
#include <cassert>
#include <cstring>
#include <map>
#include <string>

#include "../../src/Recovery/FilesystemAnalyzer.h"
#include "../../src/Recovery/RecoveryCapability.h"
#include "../../src/Recovery/RecoverySafetyPolicy.h"
#include "../../src/Disk/IDiskReader.hpp"

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

        if (startSector != 0) {
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
    // FAT12/FAT16 signature bytes at offset 0x52
    bootSector[0x52] = std::byte{0x29};  // Extended boot signature
    // Volume ID (placeholder)
    bootSector[0x53] = std::byte{0x12};
    bootSector[0x54] = std::byte{0x34};
    bootSector[0x55] = std::byte{0x56};
    bootSector[0x56] = std::byte{0x78};
    // Volume label (NO NAME    )
    bootSector[0x47] = std::byte{'N'};
    bootSector[0x48] = std::byte{'O'};
    bootSector[0x49] = std::byte{' '};
    bootSector[0x4A] = std::byte{'N'};
    bootSector[0x4B] = std::byte{'A'};
    bootSector[0x4C] = std::byte{'M'};
    bootSector[0x4D] = std::byte{'E'};
    bootSector[0x4E] = std::byte{' '};
    bootSector[0x4F] = std::byte{' '};
    bootSector[0x50] = std::byte{' '};
    bootSector[0x51] = std::byte{' '};
    // FAT type string at offset 0x36
    bootSector[0x36] = std::byte{'F'};
    bootSector[0x37] = std::byte{'A'};
    bootSector[0x38] = std::byte{'T'};
    bootSector[0x39] = std::byte{' '};
    bootSector[0x3A] = std::byte{'1'};
    bootSector[0x3B] = std::byte{'2'};
    return bootSector;
}

// Helper to create NTFS boot sector signature
std::vector<std::byte> createNtfsBootSector() {
    std::vector<std::byte> bootSector(512, std::byte{0});
    // NTFS signature at offset 0x03
    bootSector[0x00] = std::byte{'N'};
    bootSector[0x01] = std::byte{'T'};
    bootSector[0x02] = std::byte{'F'};
    bootSector[0x03] = std::byte{'S'};
    // OEM ID (empty)
    // ... rest zeros
    return bootSector;
}

// Helper to create empty boot sector
std::vector<std::byte> createEmptyBootSector() {
    return std::vector<std::byte>(512, std::byte{0});
}

// Test FilesystemAnalyzer initialization
void test_filesystem_analyzer_initialization() {
    std::cout << "Testing FilesystemAnalyzer initialization..." << std::endl;

    auto mockReader = std::make_shared<TestMockDiskReader>();
    mockReader->open("test");

    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;

    // Test constructor with all parameters
    recoverysuite::recovery::FilesystemAnalyzer analyzer(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    // Test constructor with default safety policy
    recoverysuite::recovery::FilesystemAnalyzer analyzer2(
        capabilityRegistry,
        recoverysuite::recovery::RecoverySafetyPolicy(),
        mockReader.get()
    );

    // Test constructor with null diskReader (should work, just won't be able to read)
    recoverysuite::recovery::FilesystemAnalyzer analyzer3(
        capabilityRegistry,
        safetyPolicy,
        nullptr
    );

    mockReader->close();
    std::cout << "[PASS] FilesystemAnalyzer initialization test passed" << std::endl;
}

// Test FilesystemAnalyzer analyzeFilesystems method with various inputs
void test_filesystem_analyzer_analysis() {
    std::cout << "Testing FilesystemAnalyzer analysis..." << std::endl;

    auto mockReader = std::make_shared<TestMockDiskReader>();
    mockReader->open("test");

    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;

    // Test 1: Empty boot sector (should analyze but return limited info)
    mockReader = std::make_shared<TestMockDiskReader>(createEmptyBootSector());
    mockReader->open("test");
    recoverysuite::recovery::FilesystemAnalyzer analyzer(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    std::map<std::string, std::string> analysisResults;
    bool result = analyzer.analyzeFilesystem(
        false,  // sourceIsReadOnly
        true,   // destinationDiffersFromSource
        0,      // startSector
        1,      // numSectors
        true,   // sourceRangesAreValid
        true,   // outputPathIsValid
        true,   // arithmeticCannotOverflow
        true,   // cancellationStateIsValid
        analysisResults
    );

    // Should succeed even if analysis yields limited results
    assert(result == true);
    mockReader->close();

    // Test 2: FAT boot sector
    mockReader = std::make_shared<TestMockDiskReader>(createFatBootSector());
    mockReader->open("test");
    recoverysuite::recovery::FilesystemAnalyzer analyzer2(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    std::map<std::string, std::string> analysisResults2;
    bool result2 = analyzer2.analyzeFilesystem(
        false,  // sourceIsReadOnly
        true,   // destinationDiffersFromSource
        0,      // startSector
        1,      // numSectors
        true,   // sourceRangesAreValid
        true,   // outputPathIsValid
        true,   // arithmeticCannotOverflow
        true,   // cancellationStateIsValid
        analysisResults2
    );

    assert(result2 == true);  // Analysis should succeed
    // Note: Actual analysis results depend on implementation details
    mockReader->close();

    // Test 3: NTFS boot sector
    mockReader = std::make_shared<TestMockDiskReader>(createNtfsBootSector());
    mockReader->open("test");
    recoverysuite::recovery::FilesystemAnalyzer analyzer3(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    std::map<std::string, std::string> analysisResults3;
    bool result3 = analyzer3.analyzeFilesystem(
        false,  // sourceIsReadOnly
        true,   // destinationDiffersFromSource
        0,      // startSector
        1,      // numSectors
        true,   // sourceRangesAreValid
        true,   // outputPathIsValid
        true,   // arithmeticCannotOverflow
        true,   // cancellationStateIsValid
        analysisResults3
    );

    assert(result3 == true);  // Analysis should succeed
    mockReader->close();

    std::cout << "[PASS] FilesystemAnalyzer analysis test passed" << std::endl;
}

// Test sector range validation in FilesystemAnalyzer
void test_filesystem_analyzer_sector_validation() {
    std::cout << "Testing FilesystemAnalyzer sector range validation..." << std::endl;

    auto mockReader = std::make_shared<TestMockDiskReader>();
    mockReader->open("test");

    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;
    recoverysuite::recovery::FilesystemAnalyzer analyzer(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    // Test valid ranges
    assert(analyzer.isSectorRangeValid(0, 100) == true);
    assert(analyzer.isSectorRangeValid(50, 50) == true);
    assert(analyzer.isSectorRangeValid(900, 100) == true);  // Ends at 999

    // Test invalid ranges
    assert(analyzer.isSectorRangeValid(0, 0) == false);  // Zero length
    assert(analyzer.isSectorRangeValid(1000, 1) == false);  // Beyond disk (1000+1 > 1000)
    assert(analyzer.isSectorRangeValid(900, 200) == false);  // Extends beyond disk (900+200 > 1000)

    // Test overflow protection
    assert(analyzer.isSectorRangeValid(std::numeric_limits<uint64_t>::max(), 1) == false);
    assert(analyzer.isSectorRangeValid(std::numeric_limits<uint64_t>::max() - 100, 200) == false);

    mockReader->close();
    std::cout << "[PASS] FilesystemAnalyzer sector range validation test passed" << std::endl;
}

// Test RecoveryValidationReport for recovery validation behavior
void test_recovery_validation_report() {
    std::cout << "Testing RecoveryValidationReport..." << std::endl;

    // Test default construction
    recoverysuite::recovery::RecoveryValidationReport report;
    // Note: We can't easily test the internal state without getters,
    // but we can verify it constructs without error

    std::cout << "[PASS] RecoveryValidationReport test passed" << std::endl;
}

int main() {
    std::cout << "Testing FilesystemAnalyzer and recovery validation components..." << std::endl;

    try {
        test_filesystem_analyzer_initialization();
        test_filesystem_analyzer_analysis();
        test_filesystem_analyzer_sector_validation();
        test_recovery_validation_report();

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