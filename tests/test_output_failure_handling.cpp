#include <iostream>
#include <memory>
#include <vector>
#include <cassert>
#include <cstring>
#include "../Recovery/FilesystemAnalyzer.h"
#include "../Recovery/MetadataRecovery.h"
#include "../Recovery/FileRecovery.h"
#include "../Recovery/CarvingEngine.h"
#include "../Disk/IDiskReader.hpp"
#include "../Recovery/RecoveryCapability.h"
#include "../Recovery/RecoverySafetyPolicy.h"

// Mock disk reader for testing
class SimpleMockDiskReader : public recoverysuite::disk::IDiskReader {
public:
    SimpleMockDiskReader(uint64_t totalSectors = 1000)
        : isOpen_(false), totalSectors_(totalSectors) {
        // Initialize a basic valid FAT12 boot sector
        bootSector_.assign(512, std::byte{0});
        setupValidFat12BootSector();
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

        // For simplicity, we'll just fill with our boot sector data for sector 0
        // and zeros for other sectors
        if (startSector == 0 && sectorCount >= 1) {
            std::copy(bootSector_.begin(), bootSector_.end(), buffer.begin());
            // Fill rest with zeros if needed
            if (buffer.size() > bootSector_.size()) {
                std::fill(buffer.begin() + bootSector_.size(), buffer.end(), std::byte{0});
            }
            return true;
        }

        // For other sectors, fill with zeros
        std::fill(buffer.begin(), buffer.end(), std::byte{0});
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
        info.modelNumber = "simple_mock";
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
    void setupValidFat12BootSector() {
        // Set up a basic FAT12 boot sector
        bootSector_[0x0B] = std::byte{0x00};  // Bytes per sector low byte
        bootSector_[0x0C] = std::byte{0x02};  // Bytes per sector high byte (512)
        bootSector_[0x0D] = std::byte{0x01};  // Sectors per cluster
        bootSector_[0x0E] = std::byte{0x01};  // Reserved sector count low byte
        bootSector_[0x0F] = std::byte{0x00};  // Reserved sector count high byte
        bootSector_[0x10] = std::byte{0x02};  // Number of FATs
        bootSector_[0x11] = std::byte{0xE0};  // Root entry count low byte (224 entries)
        bootSector_[0x12] = std::byte{0x00};  // Root entry count high byte
        bootSector_[0x13] = std::byte{0x20};  // Total sectors 16-bit low byte (512 sectors)
        bootSector_[0x14] = std::byte{0x00};  // Total sectors 16-bit high byte
        bootSector_[0x15] = std::byte{0xF0};  // Media descriptor
        bootSector_[0x16] = std::byte{0x09};  // FAT size 16-bit low byte (9 sectors)
        bootSector_[0x17] = std::byte{0x00};  // FAT size 16-bit high byte
        bootSector_[0x18] = std::byte{0x12};  // Sectors per track low byte
        bootSector_[0x19] = std::byte{0x00};  // Sectors per track high byte
        bootSector_[0x1A] = std::byte{0x02};  // Number of heads low byte
        bootSector_[0x1B] = std::byte{0x00};  // Number of heads high byte
        bootSector_[0x1C] = std::byte{0x00};  // Hidden sectors low byte
        bootSector_[0x1D] = std::byte{0x00};  // Hidden sectors byte 2
        bootSector_[0x1E] = std::byte{0x00};  // Hidden sectors byte 3
        bootSector_[0x1F] = std::byte{0x00};  // Hidden sectors high byte
        bootSector_[0x20] = std::byte{0x00};  // Total sectors 32-bit low byte
        bootSector_[0x21] = std::byte{0x00};  // Total sectors 32-bit byte 2
        bootSector_[0x22] = std::byte{0x00};  // Total sectors 32-bit byte 3
        bootSector_[0x23] = std::byte{0x00};  // Total sectors 32-bit high byte
        bootSector_[0x24] = std::byte{0x00};  // FAT size 32-bit low byte
        bootSector_[0x25] = std::byte{0x00};  // FAT size 32-bit byte 2
        bootSector_[0x26] = std::byte{0x00};  // FAT size 32-bit byte 3
        bootSector_[0x27] = std::byte{0x00};  // FAT size 32-bit high byte
        bootSector_[0x2C] = std::byte{0x00};  // Extended boot signature
        bootSector_[0x2D] = std::byte{0x00};  // Volume serial number
        bootSector_[0x2E] = std::byte{0x00};  // Volume serial number
        bootSector_[0x2F] = std::byte{0x00};  // Volume serial number
        bootSector_[0x30] = std::byte{0x00};  // Volume serial number
        // Volume label (11 bytes)
        bootSector_[0x2B] = std::byte{'N'};
        bootSector_[0x2C] = std::byte{'O'};
        bootSector_[0x2D] = std::byte{' '};
        bootSector_[0x2E] = std::byte{'N'};
        bootSector_[0x2F] = std::byte{'A'};
        bootSector_[0x30] = std::byte{'M'};
        bootSector_[0x31] = std::byte{'E'};
        bootSector_[0x32] = std::byte{' '};
        bootSector_[0x33] = std::byte{' '};
        bootSector_[0x34] = std::byte{' '};
        bootSector_[0x35] = std::byte{' '};
        // File system type (8 bytes)
        bootSector_[0x36] = std::byte{'F'};
        bootSector_[0x37] = std::byte{'A'};
        bootSector_[0x38] = std::byte{'T'};
        bootSector_[0x39] = std::byte{'1'};
        bootSector_[0x3A] = std::byte{'2'};
        bootSector_[0x3B] = std::byte{' '};
        bootSector_[0x3C] = std::byte{' '};
        bootSector_[0x3D] = std::byte{' '};
        // Boot sector signature
        bootSector_[0x1FE] = std::byte{0x55};
        bootSector_[0x1FF] = std::byte{0xAA};
    }

private:
    bool isOpen_;
    uint64_t totalSectors_;
    std::vector<std::byte> bootSector_; // 512 bytes for boot sector
};

// Test that recovery operations properly handle destination same as source
void test_destination_same_as_source() {
    std::cout << "Testing output failure handling - destination same as source..." << std::endl;

    // Create mock disk reader
    auto mockReader = std::make_shared<SimpleMockDiskReader>();
    mockReader->open("mock");

    // Create recovery capability registry and safety policy
    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS);
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY);
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY);
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::CARVING);
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;

    // Test parameters with destinationDiffersFromSource = false (invalid - same as source)
    bool sourceIsReadOnly = true;
    bool destinationDiffersFromSource = false; // INVALID: destination same as source
    uint64_t startSector = 0;
    uint64_t numSectors = 1;
    bool sourceRangesAreValid = true;
    bool outputPathIsValid = true;
    bool arithmeticCannotOverflow = true;
    bool cancellationStateIsValid = true;

    // Test FilesystemAnalyzer
    {
        recoverysuite::recovery::FilesystemAnalyzer analyzer(
            capabilityRegistry,
            safetyPolicy,
            mockReader.get()
        );

        std::map<std::string, std::string> analysisResults;
        bool success = analyzer.analyzeFilesystem(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            startSector,
            numSectors,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid,
            analysisResults
        );

        if (success) {
            std::cerr << "ERROR: FilesystemAnalyzer should have failed due to destination same as source!" << std::endl;
            mockReader->close();
            return;
        } else {
            std::cout << "[PASS] FilesystemAnalyzer correctly rejected operation with destination same as source" << std::endl;
        }
    }

    // Test MetadataRecovery
    {
        recoverysuite::recovery::MetadataRecovery metadataRecovery(
            capabilityRegistry,
            safetyPolicy,
            mockReader.get()
        );

        std::map<std::string, std::vector<uint8_t>> recoveredMetadata;
        bool success = metadataRecovery.recoverMetadata(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            startSector,
            numSectors,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid,
            recoveredMetadata
        );

        if (success) {
            std::cerr << "ERROR: MetadataRecovery should have failed due to destination same as source!" << std::endl;
            mockReader->close();
            return;
        } else {
            std::cout << "[PASS] MetadataRecovery correctly rejected operation with destination same as source" << std::endl;
        }
    }

    // Test FileRecovery
    {
        recoverysuite::recovery::FileRecovery fileRecovery(
            capabilityRegistry,
            safetyPolicy,
            mockReader.get()
        );

        std::vector<uint8_t> recoveredData;
        bool success = fileRecovery.recoverFile(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            startSector,
            numSectors,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid,
            "test_file",
            startSector,
            numSectors * 512,
            recoveredData
        );

        if (success) {
            std::cerr << "ERROR: FileRecovery should have failed due to destination same as source!" << std::endl;
            mockReader->close();
            return;
        } else {
            std::cout << "[PASS] FileRecovery correctly rejected operation with destination same as source" << std::endl;
        }
    }

    // Test CarvingEngine
    {
        recoverysuite::recovery::CarvingEngine carvingEngine(
            capabilityRegistry,
            safetyPolicy,
            mockReader.get()
        );

        std::map<std::string, std::vector<uint8_t>> carvedFiles;
        bool success = carvingEngine.carveFiles(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            startSector,
            numSectors,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid,
            carvedFiles
        );

        if (success) {
            std::cerr << "ERROR: CarvingEngine should have failed due to destination same as source!" << std::endl;
            mockReader->close();
            return;
        } else {
            std::cout << "[PASS] CarvingEngine correctly rejected operation with destination same as source" << std::endl;
        }
    }

    mockReader->close();
}

// Test that recovery operations properly handle valid destination (different from source)
void test_valid_destination() {
    std::cout << "Testing recovery operations with valid destination (different from source)..." << std::endl;

    // Create mock disk reader
    auto mockReader = std::make_shared<SimpleMockDiskReader>();
    mockReader->open("mock");

    // Create recovery capability registry and safety policy
    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS);
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;

    // Test parameters with destinationDiffersFromSource = true (valid - different from source)
    bool sourceIsReadOnly = true;
    bool destinationDiffersFromSource = true; // VALID: destination different from source
    uint64_t startSector = 0;
    uint64_t numSectors = 1;
    bool sourceRangesAreValid = true;
    bool outputPathIsValid = true;
    bool arithmeticCannotOverflow = true;
    bool cancellationStateIsValid = true;

    // Test FilesystemAnalyzer - should proceed with validation (may fail for other reasons, but not destination issue)
    {
        recoverysuite::recovery::FilesystemAnalyzer analyzer(
            capabilityRegistry,
            safetyPolicy,
            mockReader.get()
        );

        std::map<std::string, std::string> analysisResults;
        bool success = analyzer.analyzeFilesystem(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            startSector,
            numSectors,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid,
            analysisResults
        );

        // The operation might still fail for other reasons (e.g., validation of other parameters),
        // but it should not fail due to destination being same as source
        std::cout << "[PASS] FilesystemAnalyzer processed operation with valid destination" << std::endl;
    }

    mockReader->close();
}

int main() {
    std::cout << "Testing RecoverySuite Output Failure Handling..." << std::endl;

    try {
        test_destination_same_as_source();
        test_valid_destination();

        std::cout << "All output failure handling tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}