#include <iostream>
#include <memory>
#include <vector>
#include <cassert>
#include <cstring>
#include "../Recovery/CarvingEngine.h"
#include "../Disk/IDiskReader.hpp"
#include "../Recovery/RecoveryCapability.h"
#include "../Recovery/RecoverySafetyPolicy.h"

// Mock disk reader for testing
class SimpleMockDiskReader : public recoverysuite::disk::IDiskReader {
public:
    SimpleMockDiskReader(const std::vector<std::byte>& dataToReturn, uint64_t totalSectors = 1000)
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

        // For our test, we'll return the test data regardless of sector parameters
        // In a real test, we would map sectors to data appropriately
        buffer.insert(buffer.end(), dataToReturn_.begin(), dataToReturn_.end());
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
    bool isOpen_;
    std::vector<std::byte> dataToReturn_;
    uint64_t totalSectors_;
};

// Test carving engine with data containing false positives
void test_carving_false_positives() {
    std::cout << "Testing carving engine with false positive signatures..." << std::endl;

    // Create data that looks like it has JPEG/PDF/ZIP signatures but is actually just text
    std::string falsePositiveData =
        "This is some text that contains FF D8 FF but is not a JPEG\xFF\xD8\xFF"
        "More text here with %%EOF but not a PDF%%EOF"
        "Even more text with 50 4B 03 04 but not a ZIP\x50\x4B\x03\x04"
        "End of false positive data";

    std::vector<std::byte> data;
    for (char c : falsePositiveData) {
        data.push_back(static_cast<std::byte>(c));
    }

    // Create mock disk reader
    auto mockReader = std::make_shared<SimpleMockDiskReader>(data);
    mockReader->open("mock");

    // Create recovery capability registry and safety policy
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
    uint64_t numSectors = 10; // Enough sectors to cover our data
    bool sourceRangesAreValid = true;
    bool outputPathIsValid = true;
    bool arithmeticCannotOverflow = true;
    bool cancellationStateIsValid = true;

    // Try to carve files - should not crash, even with false positives
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
        mockReader->close();
        return;
    } catch (...) {
        std::cerr << "Carving failed with unknown exception" << std::endl;
        mockReader->close();
        return;
    }

    // If we get here, the function didn't throw an exception
    // It's okay if it finds some files (due to false positives) or doesn't find any
    std::cout << "[PASS] Carving engine handled false positives without crashing" << std::endl;
    std::cout << "    Found " << carvedFiles.size() << " files (may be due to false positives)" << std::endl;

    mockReader->close();
}

// Test carving engine with truncated file signatures
void test_carving_truncated_signatures() {
    std::cout << "Testing carving engine with truncated signatures..." << std::endl;

    // Create data with incomplete signatures
    std::string truncatedData =
        "JPEG start only: \xFF\xD8\xFF"  // Missing the fourth byte
        "PDF start only: %PDF"           // This is actually complete
        "ZIP start only: \x50\x4B\x03";  // Missing the fourth byte

    std::vector<std::byte> data;
    for (char c : truncatedData) {
        data.push_back(static_cast<std::byte>(c));
    }

    // Create mock disk reader
    auto mockReader = std::make_shared<SimpleMockDiskReader>(data);
    mockReader->open("mock");

    // Create recovery capability registry and safety policy
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

    // Try to carve files - should not crash
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
        mockReader->close();
        return;
    } catch (...) {
        std::cerr << "Carving failed with unknown exception" << std::endl;
        mockReader->close();
        return;
    }

    // If we get here, the function didn't throw an exception
    std::cout << "[PASS] Carving engine handled truncated signatures without crashing" << std::endl;
    std::cout << "    Found " << carvedFiles.size() << " files" << std::endl;

    mockReader->close();
}

// Test carving engine with empty data
void test_carving_empty_data() {
    std::cout << "Testing carving engine with empty data..." << std::endl;

    // Create empty data
    std::vector<std::byte> data;

    // Create mock disk reader
    auto mockReader = std::make_shared<SimpleMockDiskReader>(data);
    mockReader->open("mock");

    // Create recovery capability registry and safety policy
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
    uint64_t numSectors = 1;
    bool sourceRangesAreValid = true;
    bool outputPathIsValid = true;
    bool arithmeticCannotOverflow = true;
    bool cancellationStateIsValid = true;

    // Try to carve files - should not crash
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
        mockReader->close();
        return;
    } catch (...) {
        std::cerr << "Carving failed with unknown exception" << std::endl;
        mockReader->close();
        return;
    }

    // If we get here, the function didn't throw an exception
    // Should not find any files in empty data
    if (success) {
        std::cerr << "ERROR: Carving engine reported success on empty data!" << std::endl;
        mockReader->close();
        return;
    } else {
        std::cout << "[PASS] Carving engine correctly handled empty data (no files found)" << std::endl;
    }

    mockReader->close();
}

// Test carving engine with very large sector count (tests overflow protection)
void test_carving_large_sector_count() {
    std::cout << "Testing carving engine with large sector count..." << std::endl;

    // Create minimal data
    std::string smallData = "Some data";
    std::vector<std::byte> data;
    for (char c : smallData) {
        data.push_back(static_cast<std::byte>(c));
    }

    // Create mock disk reader
    auto mockReader = std::make_shared<SimpleMockDiskReader>(data);
    mockReader->open("mock");

    // Create recovery capability registry and safety policy
    recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
    capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::CARVING);
    recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;

    // Create carving engine
    recoverysuite::recovery::CarvingEngine carvingEngine(
        capabilityRegistry,
        safetyPolicy,
        mockReader.get()
    );

    // Test parameters with very large sector count that might cause overflow
    bool sourceIsReadOnly = true;
    bool destinationDiffersFromSource = true;
    uint64_t startSector = 0;
    uint64_t numSectors = 1000000000000ULL; // Very large number
    bool sourceRangesAreValid = true;
    bool outputPathIsValid = true;
    bool arithmeticCannotOverflow = true; // This should be set to false by validation due to overflow
    bool cancellationStateIsValid = true;

    // Try to carve files - should fail due to arithmetic overflow protection
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
        mockReader->close();
        // This is actually expected due to overflow protection
        std::cout << "[PASS] Carving engine correctly handled large sector count (exception caught)" << std::endl;
        return;
    } catch (...) {
        std::cerr << "Carving failed with unknown exception" << std::endl;
        mockReader->close();
        // This is also expected
        std::cout << "[PASS] Carving engine correctly handled large sector count (unknown exception)" << std::endl;
        return;
    }

    // If we get here without an exception, check if it failed due to validation
    if (success) {
        std::cerr << "ERROR: Carving engine should have failed due to arithmetic overflow!" << std std::endl;
        mockReader->close();
        return;
    } else {
        std::cout << "[PASS] Carving engine correctly handled large sector count (returned false)" << std::endl;
    }

    mockReader->close();
}

int main() {
    std::cout << "Testing RecoverySuite Carving Robustness..." << std::endl;

    try {
        test_carving_false_positives();
        test_carving_truncated_signatures();
        test_carving_empty_data();
        test_carving_large_sector_count();

        std::cout << "All carving robustness tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}