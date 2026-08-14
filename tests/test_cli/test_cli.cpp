#include <iostream>
#include <memory>
#include <vector>
#include <cassert>
#include <cstring>
#include "../../src/CLI/CLIHandler.h"
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
        info.serialNumber = "TEST123";
        info.firmwareRevision = "1.0";
        info.bytesPerSector = 512;
        info.totalSectors = totalSectors_;
        info.totalSectors = totalSectors_; // This line is redundant but harmless
        info.isRemovable = false;
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

// Mock disk manager for testing
class TestMockDiskManager {
public:
    std::vector<recoverysuite::disk::DeviceInformation> enumerateDisks() {
        std::vector<recoverysuite::disk::DeviceInformation> disks;

        recoverysuite::disk::DeviceInformation info1;
        info1.modelNumber = "MockDisk1";
        info1.serialNumber = "MOCK001";
        info1.firmwareRevision = "1.0";
        info1.bytesPerSector = 512;
        info1.totalSectors = 1000;
        info1.isRemovable = false;
        disks.push_back(info1);

        recoverysuite::disk::DeviceInformation info2;
        info2.modelNumber = "MockDisk2";
        info2.serialNumber = "MOCK002";
        info2.firmwareRevision = "1.0";
        info2.bytesPerSector = 512;
        info2.totalSectors = 2000;
        info2.isRemovable = true;
        disks.push_back(info2);

        return disks;
    }

    recoverysuite::disk::DeviceInformation getDiskInfo(uint64_t diskNumber) {
        auto disks = enumerateDisks();
        if (diskNumber >= disks.size()) {
            throw recoverysuite::disk::DiskNotFoundException("Disk not found");
        }
        return disks[diskNumber];
    }

    std::unique_ptr<recoverysuite::disk::IDiskReader> openDisk(uint64_t diskNumber, bool readOnly = true) {
        // Return a mock disk reader with some test data
        std::vector<std::byte> testData(512, std::byte{0xAA}); // Test pattern
        return std::make_unique<TestMockDiskReader>(testData, 1000);
    }

    uint64_t getDiskCount() {
        return enumerateDisks().size();
    }
};

// Test basic CLI functionality
void test_cli_help() {
    std::cout << "Testing CLI help output..." << std::endl;

    // We can't easily test the actual CLI output without capturing stdout
    // For now, we'll just verify the CLIHandler can be instantiated
    recoverysuite::cli::CLIHandler cli;
    std::cout << "[PASS] CLIHandler instantiated successfully" << std::endl;
}

// Test command parsing
void test_command_parsing() {
    std::cout << "Testing command parsing..." << std::endl;

    recoverysuite::cli::CLIHandler cli;

    // Test valid capability parsing
    assert(cli.parseCapability("filesystem_analysis") == recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS);
    assert(cli.parseCapability("file_recovery") == recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY);
    assert(cli.parseCapability("metadata_recovery") == recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY);
    assert(cli.parseCapability("carving") == recoverysuite::recovery::RecoveryCapability::CARVING);

    // Test case insensitivity
    assert(cli.parseCapability("FILESYSTEM_ANALYSIS") == recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS);
    assert(cli.parseCapability("File_Recovery") == recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY);

    // Test invalid capability
    assert(cli.parseCapability("invalid_capability") == recoverysuite::recovery::RecoveryCapability::NONE);

    std::cout << "[PASS] Command parsing tests passed" << std::endl;
}

// Test uint64 parsing
void test_uint64_parsing() {
    std::cout << "Testing uint64 parsing..." << std::endl;

    recoverysuite::cli::CLIHandler cli;

    uint64_t value;
    assert(cli.parseUint64("0", value) && value == 0);
    assert(cli.parseUint64("123", value) && value == 123);
    assert(cli.parseUint64("18446744073709551615", value) && value == 18446744073709551615ULL);

    // Test invalid inputs
    assert(!cli.parseUint64("", value));
    assert(!cli.parseUint64("abc", value));
    assert(!cli.parseUint64("123abc", value));
    assert(!cli.parseUint64("-1", value));

    std::cout << "[PASS] Uint64 parsing tests passed" << std::endl;
}

int main() {
    std::cout << "Testing RecoverySuite CLI..." << std::endl;

    try {
        test_cli_help();
        test_command_parsing();
        test_uint64_parsing();

        std::cout << "All CLI tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}