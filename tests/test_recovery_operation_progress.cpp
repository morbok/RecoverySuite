#include <iostream>
#include <memory>
#include <vector>
#include <cassert>
#include <string>
#include <functional>
#include <chrono>
#include <thread>

#include "../../../src/Application/Service/GUIRecoveryService.h"
#include "../../../src/Application/Service/Models/GUIModels.h"
#include "../../../src/Recovery/RecoveryOperationValidator.h"
#include "../../../src/Recovery/RecoveryCapability.h"
#include "../../../src/Recovery/RecoverySafetyPolicy.h"
#include "../../../src/Disk/IDiskReader.hpp"
#include "../../../src/GUI/core/MainWindow.h"
#include "../../../src/GUI/widgets/OperationProgressWidget.h"
#include <QApplication>

// Mock disk reader for testing
class TestMockDiskReader : public recoverysuite::disk::IDiskReader {
public:
    TestMockDiskReader(const std::vector<std::byte>& dataToReturn = {}, uint64_t totalSectors = 1000)
        : isOpen_(false), dataToReturn_(dataToReturn), totalSectors_(totalSectors) {
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

        const uint32_t sectorSize = 512;
        size_t bufferSizeNeeded = static_cast<size_t>(sectorCount) * static_cast<size_t>(sectorSize);

        if (bufferSizeNeeded == 0) {
            return false;
        }

        buffer.resize(bufferSizeNeeded);

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
        if (!isOpen_) {
            throw recoverysuite::disk::DiskException("Disk is not open");
        }
        return true;
    }

    recoverysuite::disk::DeviceInformation getDiskInfo() const override {
        recoverysuite::disk::DeviceInformation info;
        info.modelNumber = "test_mock";
        info.serialNumber = "TEST123";
        info.bytesPerSector = 512;
        info.totalSectors = totalSectors_;
        info.isReadOnly = false; // Add isReadOnly field
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

// Test the recovery operation progress interface
void test_recovery_operation_progress_interface() {
    std::cout << "Testing Recovery Operation Progress Interface..." << std::endl;

    // Create QApplication if not already created
    static int argc = 0;
    static char* argv[] = { nullptr };
    QApplication app(argc, argv);

    // Create mock disk reader
    auto mockReader = std::make_shared<TestMockDiskReader>();
    mockReader->open("test");

    // Create GUI recovery service
    recoverysuite::application::service::GUIRecoveryService service(mockReader);

    // Create main window
    recoverysuite::gui::core::MainWindow mainWindow(&service);

    // Access the progress widget (we can't directly access private members, so we use the public interface)
    // We'll test by simulating a recovery operation and checking the progress updates.

    // For simplicity, we'll test the OperationProgressWidget directly
    recoverysuite::gui::widgets::OperationProgressWidget progressWidget;

    // Test initial state
    // We'll just call the update methods to ensure they don't crash.

    // Test basic update
    progressWidget.updateProgress("Test Operation", 50, "Running...");

    // Test detailed update
    progressWidget.updateProgress("Test Operation", 75, "Reading sectors...", 1024, 2048, 10, 5, 0, true);

    // Reset progress
    progressWidget.resetProgress();

    std::cout << "[PASS] Recovery operation progress interface test completed" << std::endl;

    mockReader->close();
}

int main() {
    std::cout << "Testing Recovery Operation Progress Functionality..." << std::endl;

    try {
        test_recovery_operation_progress_interface();
        std::cout << "All recovery operation progress tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}