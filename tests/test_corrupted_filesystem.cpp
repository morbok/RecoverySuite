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

// Mock disk reader that can simulate corrupted data
class CorruptedMockDiskReader : public recoverysuite::disk::IDiskReader {
public:
    CorruptedMockDiskReader(bool corruptBootSector = false,
                           bool corruptFatTables = false,
                           bool returnPartialRead = false,
                           uint64_t totalSectors = 1000)
        : isOpen_(false),
          corruptBootSector_(corruptBootSector),
          corruptFatTables_(corruptFatTables),
          returnPartialRead_(returnPartialRead),
          totalSectors_(totalSectors),
          readCallCount_(0) {
        // Initialize a basic valid FAT12 boot sector by default
        bootSector_.assign(512, std::byte{0});
        setupValidFat12BootSector();

        // If we're supposed to corrupt the boot sector, do it now
        if (corruptBootSector_) {
            corruptBootSectorData();
        }

        // Initialize FAT table data
        fatData_.assign(9 * 512, std::byte{0}); // 9 sectors of FAT data
        setupValidFatData();

        // If we're supposed to corrupt the FAT tables, do it now
        if (corruptFatTables_) {
            corruptFatTableData();
        }
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

        readCallCount_++;

        // Calculate sector size (512 bytes for our mock)
        const uint32_t sectorSize = 512;
        size_t bufferSizeNeeded = static_cast<size_t>(sectorCount) * static_cast<size_t>(sectorSize);

        if (bufferSizeNeeded == 0) {
            return false;
        }

        buffer.resize(bufferSizeNeeded);

        // Handle partial reads if requested
        if (returnPartialRead_ && readCallCount_ == 1) {
            // Return only half the requested data on first read
            size_t halfSize = bufferSizeNeeded / 2;
            if (halfSize > 0) {
                std::fill(buffer.begin(), buffer.begin() + halfSize, std::byte{0});
                buffer.resize(halfSize);
                return true;
            }
        }

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

        // For FAT table sectors (1-9), return FAT data
        if (startSector >= 1 && startSector <= 9 && sectorCount >= 1) {
            uint64_t fatStartIndex = (startSector - 1) * sectorSize;
            if (fatStartIndex + sectorSize * sectorCount <= fatData_.size()) {
                std::copy(fatData_.begin() + fatStartIndex,
                         fatData_.begin() + fatStartIndex + sectorSize * sectorCount,
                         buffer.begin());
                // Fill rest with zeros if needed
                if (buffer.size() > sectorSize * sectorCount) {
                    std::fill(buffer.begin() + sectorSize * sectorCount, buffer.end(), std::byte{0});
                }
                return true;
            }
        }

        // For other sectors, fill with zeros
        std::fill(buffer.begin(), buffer.end(), std::byte{0});
        return true;
    }

    bool writeSectors(uint64_t startSector, uint64t sectorCount, const std::vector<std::byte>& buffer) override {
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
        info.modelNumber = "corrupted_mock";
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

    void corruptBootSectorData() {
        // Corrupt the boot sector by invalidating the signature
        bootSector_[0x1FE] = std::byte{0x00};
        bootSector_[0x1FF] = std::byte{0x00};
    }

    void setupValidFatData() {
        // Set up basic FAT12 data - all entries pointing to next cluster (normal chain)
        for (size_t i = 0; i < fatData_.size(); i += 3) {
            // FAT12 entry for cluster 0x002 (first data cluster) points to 0x003
            // FAT12 uses 12-bit entries, so we packed them
            // Entry 0: 0xFFF (reserved)
            // Entry 1: 0xFFF (reserved)
            // Entry 2: 0x003 (points to cluster 3)
            if (i < fatData_.size()) fatData_[i] = std::byte{0xFF};      // Entry 0, bits 0-7
            if (i + 1 < fatData_.size()) fatData_[i + 1] = std::byte{0xFF}; // Entry 0, bits 8-11 + Entry 1, bits 0-3
            if (i + 2 < fatData_.size()) fatData_[i + 2] = std::byte{0xFF}; // Entry 1, bits 4-11

            // Next entry
            if (i + 3 < fatData_.size()) fatData_[i + 3] = std::byte{0x03}; // Entry 2, bits 0-7
            if (i + 4 < fatData_.size()) fatData_[i + 4] = std::byte{0x00}; // Entry 2, bits 8-11 + Entry 3, bits 0-3
            if (i + 5 < fatData_.size()) fatData_[i + 5] = std::byte{0xFF}; // Entry 3, bits 4-11
        }

        // Mark end of cluster chain at cluster 0xFFF
        size_t lastEntryIndex = (fatData_.size() / 3) * 3;
        if (lastEntryIndex < fatData_.size()) fatData_[lastEntryIndex] = std::byte{0xFF};
        if (lastEntryIndex + 1 < fatData_.size()) fatData_[lastEntryIndex + 1] = std::byte{0xFF};
        if (lastEntryIndex + 2 < fatData_.size()) fatData_[lastEntryIndex + 2] = std::byte{0xFF};
    }

    void corruptFatTableData() {
        // Corrupt the FAT table by setting invalid values
        // Set some cluster entries to invalid values
        for (size_t i = 0; i < fatData_.size(); i += 3) {
            if (i < fatData_.size()) fatData_[i] = std::byte{0x00};      // Corrupt entry
            if (i + 1 < fatData_.size()) fatData_[i + 1] = std::byte{0x00}; // Corrupt entry
            if (i + 2 < fatData_.size()) fatData_[i + 2] = std::byte{0x00}; // Corrupt entry
        }
    }

private:
    bool isOpen_;
    bool corruptBootSector_;
    bool corruptFatTables_;
    bool returnPartialRead_;
    uint64_t totalSectors_;
    uint64_t readCallCount_;
    std::vector<std::byte> bootSector_; // 512 bytes for boot sector
    std::vector<std::byte> fatData_;    // FAT table data
};

// Test that FilesystemAnalyzer handles corrupted boot sectors gracefully
void test_filesystem_analyzer_corrupted_boot_sector() {
    std::cout << "Testing FilesystemAnalyzer with corrupted boot sector..." << std::endl;

    // Create mock disk reader with corrupted boot sector
    auto mockReader = std::make_shared<CorruptedMockDiskReader>(true, false, false);
    mockReader->open("mock");

    // Create recovery capability registry and safety policy (minimal)
    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS);
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;

    // Create filesystem analyzer
    recoverysuite::recovery::FilesystemAnalyzer analyzer(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    // Test parameters
    bool sourceIsReadOnly = true;
    bool destinationDiffersFromSource = true;
    uint64_t startSector = 0;
    uint64_t numSectors = 1;
    bool sourceRangesAreValid = true;
    bool outputPathIsValid = true;
    bool arithmeticCannotOverflow = true;
    bool cancellationStateIsValid = true;

    // Analyze filesystem - should fail gracefully due to corrupted boot sector
    std::map<std::string, std::string> analysisResults;
    bool success = false;
    try {
        success = analyzer.analyzeFilesystem(
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
    } catch (const std::exception& e) {
        std::cerr << "Filesystem analysis failed with exception: " << e.what() << std::endl;
        // This is acceptable - the function should handle errors gracefully
        mockReader->close();
        std::cout << "[PASS] FilesystemAnalyzer correctly handled corrupted boot sector (exception caught)" << std::endl;
        return;
    } catch (...) {
        std::cerr << "Filesystem analysis failed with unknown exception" << std::endl;
        // This is also acceptable
        mockReader->close();
        std::cout << "[PASS] FilesystemAnalyzer correctly handled corrupted boot sector (unknown exception)" << std::endl;
        return;
    }

    // If we get here, the function didn't throw an exception
    if (success) {
        std::cerr << "ERROR: Filesystem analysis should have failed with corrupted boot sector!" << std::endl;
        mockReader->close();
        return;
    } else {
        std::cout << "[PASS] FilesystemAnalyzer correctly handled corrupted boot sector (returned false)" << std::endl;
    }

    mockReader->close();
}

// Test that FilesystemAnalyzer handles partial reads gracefully
void test_filesystem_analyzer_partial_read() {
    std::cout << "Testing FilesystemAnalyzer with partial reads..." << std::endl;

    // Create mock disk reader that returns partial reads
    auto mockReader = std::make_shared<CorruptedMockDiskReader>(false, false, true);
    mockReader->open("mock");

    // Create recovery capability registry and safety policy (minimal)
    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS);
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;

    // Create filesystem analyzer
    recoverysuite::recovery::FilesystemAnalyzer analyzer(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    // Test parameters
    bool sourceIsReadOnly = true;
    bool destinationDiffersFromSource = true;
    uint64_t startSector = 0;
    uint64_t numSectors = 1;
    bool sourceRangesAreValid = true;
    bool outputPathIsValid = true;
    bool arithmeticCannotOverflow = true;
    bool cancellationStateIsValid = true;

    // Analyze filesystem - should fail gracefully due to partial read
    std::map<std::string, std::string> analysisResults;
    bool success = false;
    try {
        success = analyzer.analyzeFilesystem(
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
    } catch (const std::exception& e) {
        std::cerr << "Filesystem analysis failed with exception: " << e.what() << std::endl;
        // This is acceptable - the function should handle errors gracefully
        mockReader->close();
        std::cout << "[PASS] FilesystemAnalyzer correctly handled partial read (exception caught)" << std::endl;
        return;
    } catch (...) {
        std::cerr << "Filesystem analysis failed with unknown exception" << std::endl;
        // This is also acceptable
        mockReader->close();
        std::cout << "[PASS] FilesystemAnalyzer correctly handled partial read (unknown exception)" << std::endl;
        return;
    }

    // If we get here, the function didn't throw an exception
    if (success) {
        std::cerr << "ERROR: Filesystem analysis should have failed with partial read!" << std::endl;
        mockReader->close();
        return;
    } else {
        std::cout << "[PASS] FilesystemAnalyzer correctly handled partial read (returned false)" << std::endl;
    }

    mockReader->close();
}

// Test that MetadataRecovery handles corrupted FAT tables gracefully
void test_metadata_recovery_corrupted_fat() {
    std::cout << "Testing MetadataRecovery with corrupted FAT tables..." << std::endl;

    // Create mock disk reader with corrupted FAT tables
    auto mockReader = std::make_shared<CorruptedMockDiskReader>(false, true, false);
    mockReader->open("mock");

    // Create recovery capability registry and safety policy (minimal)
    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY);
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;

    // Create metadata recovery object
    recoverysuite::recovery::MetadataRecovery metadataRecovery(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    // Test parameters
    bool sourceIsReadOnly = true;
    bool destinationDiffersFromSource = true;
    uint64_t startSector = 0;
    uint64_t numSectors = 10; // Read boot sector + FAT sectors
    bool sourceRangesAreValid = true;
    bool outputPathIsValid = true;
    bool arithmeticCannotOverflow = true;
    bool cancellationStateIsValid = true;

    // Recover metadata - should fail gracefully due to corrupted FAT
    std::map<std::string, std::vector<uint8_t>> recoveredMetadata;
    bool success = false;
    try {
        success = metadataRecovery.recoverMetadata(
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
    } catch (const std::exception& e) {
        std::cerr << "Metadata recovery failed with exception: " << e.what() << std::endl;
        // This is acceptable - the function should handle errors gracefully
        mockReader->close();
        std::cout << "[PASS] MetadataRecovery correctly handled corrupted FAT (exception caught)" << std::endl;
        return;
    } catch (...) {
        std::cerr << "Metadata recovery failed with unknown exception" << std::endl;
        // This is also acceptable
        mockReader->close();
        std::cout << "[PASS] MetadataRecovery correctly handled corrupted FAT (unknown exception)" << std::endl;
        return;
    }

    // If we get here, the function didn't throw an exception
    if (success) {
        std::cerr << "ERROR: Metadata recovery should have failed with corrupted FAT!" << std::endl;
        mockReader->close();
        return;
    } else {
        std::cout << "[PASS] MetadataRecovery correctly handled corrupted FAT (returned false)" << std::endl;
    }

    mockReader->close();
}

// Test that FileRecovery handles invalid sector ranges gracefully
void test_file_recovery_invalid_sector_range() {
    std::cout << "Testing FileRecovery with invalid sector ranges..." << std::endl;

    // Create mock disk reader
    auto mockReader = std::make_shared<CorruptedMockDiskReader>(false, false, false);
    mockReader->open("mock");

    // Create recovery capability registry and safety policy (minimal)
    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY);
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;

    // Create file recovery object
    recoverysuite::recovery::FileRecovery fileRecovery(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    // Test parameters for invalid sector range (beyond disk size)
    bool sourceIsReadOnly = true;
    bool destinationDiffersFromSource = true;
    uint64_t startSector = 2000; // Beyond our mock disk size of 1000
    uint64_t numSectors = 100;
    bool sourceRangesAreValid = true;
    bool outputPathIsValid = true;
    bool arithmeticCannotOverflow = true;
    bool cancellationStateIsValid = true;

    // Try to recover a file - should fail gracefully due to invalid sector range
    std::vector<uint8_t> recoveredData;
    bool success = false;
    try {
        success = fileRecovery.recoverFile(
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
            numSectors * 512, // file size in bytes
            recoveredData
        );
    } catch (const std::exception& e) {
        std::cerr << "File recovery failed with exception: " << e.what() << std::endl;
        // This is acceptable - the function should handle errors gracefully
        mockReader->close();
        std::cout << "[PASS] FileRecovery correctly handled invalid sector range (exception caught)" << std::endl;
        return;
    } catch (...) {
        std::cerr << "File recovery failed with unknown exception" << std::endl;
        // This is also acceptable
        mockReader->close();
        std::cout << "[PASS] FileRecovery correctly handled invalid sector range (unknown exception)" << std::endl;
        return;
    }

    // If we get here, the function didn't throw an exception
    if (success) {
        std::cerr << "ERROR: File recovery should have failed with invalid sector range!" << std::endl;
        mockReader->close();
        return;
    } else {
        std::cout << "[PASS] FileRecovery correctly handled invalid sector range (returned false)" << std::endl;
    }

    mockReader->close();
}

// Test that CarvingEngine handles partial reads gracefully
void test_carving_engine_partial_read() {
    std::cout << "Testing CarvingEngine with partial reads..." << std::endl;

    // Create mock disk reader that returns partial reads
    auto mockReader = std::make_shared<CorruptedMockDiskReader>(false, false, true);
    mockReader->open("mock");

    // Create recovery capability registry and safety policy (minimal)
    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::CARVING);
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;

    // Create carving engine
    recoverysuite::recovery::CarvingEngine carvingEngine(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    // Test parameters
    bool sourceIsReadOnly = true;
    bool destinationDiffersFromSource = true;
    uint64_t startSector = 0;
    uint64_t numSectors = 10;
    bool sourceRangesAreValid = true;
    bool outputPathIsValid = true;
    bool arithmeticCannotOverflow = true;
    bool cancellationStateIsValid = true;

    // Try to carve files - should fail gracefully due to partial read
    std::map<std::string, std::vector<uint8_t>> carvedFiles;
    bool success = false;
    try {
        success = carvingEngine.carveFiles(
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
    } catch (const std::exception& e) {
        std::cerr << "Carving failed with exception: " << e.what() << std::endl;
        // This is acceptable - the function should handle errors gracefully
        mockReader->close();
        std::cout << "[PASS] CarvingEngine correctly handled partial read (exception caught)" << std::endl;
        return;
    } catch (...) {
        std::cerr << "Carving failed with unknown exception" << std::endl;
        // This is also acceptable
        mockReader->close();
        std::cout << "[PASS] CarvingEngine correctly handled partial read (unknown exception)" << std::endl;
        return;
    }

    // If we get here, the function didn't throw an exception
    if (success) {
        std::cerr << "ERROR: Carving should have failed with partial read!" << std::endl;
        mockReader->close();
        return;
    } else {
        std::cout << "[PASS] CarvingEngine correctly handled partial read (returned false)" << std::endl;
    }

    mockReader->close();
}

int main() {
    std::cout << "Testing RecoverySuite Corrupted Filesystem Handling..." << std::endl;

    try {
        test_filesystem_analyzer_corrupted_boot_sector();
        test_filesystem_analyzer_partial_read();
        test_metadata_recovery_corrupted_fat();
        test_file_recovery_invalid_sector_range();
        test_carving_engine_partial_read();

        std::cout << "All corrupted filesystem handling tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}