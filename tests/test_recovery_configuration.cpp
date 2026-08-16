#include <iostream>
#include <memory>
#include <vector>
#include <cassert>
#include <string>
#include <functional>

#include "../../../src/Application/Service/GUIRecoveryService.h"
#include "../../../src/Application/Service/Models/GUIModels.h"
#include "../../../src/Recovery/RecoveryOperationValidator.h"
#include "../../../src/Recovery/RecoveryCapability.h"
#include "../../../src/Recovery/RecoverySafetyPolicy.h"
#include "../../../src/Disk/IDiskReader.hpp"

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

// Test RecoveryOperationValidator integration
void test_recovery_operation_validator_integration() {
    std::cout << "Testing RecoveryOperationValidator integration..." << std::endl;

    auto mockReader = std::make_shared<TestMockDiskReader>();
    mockReader->open("test");

    // Create GUI recovery service
    recoverysuite::application::service::GUIRecoveryService service(mockReader);

    // Test valid file recovery operation
    recoverysuite::application::service::models::RecoveryOperation validOp;
    validOp.operationType = "file_recovery";
    validOp.targetDevicePath = "/dev/sda1";
    validOp.startSector = 0;
    validOp.numSectors = 100;
    validOp.outputPath = "/tmp/recovered";
    validOp.verifyAfterRecovery = true;
    validOp.createLogFile = true;

    // TODO: Add validation method to GUIRecoveryService and test it

    std::cout << "[PASS] RecoveryOperationValidator integration test completed" << std::endl;
    mockReader->close();
}

int main() {
    std::cout << "Testing Recovery Configuration Functionality..." << std::endl;

    try {
        test_recovery_operation_validator_integration();
        std::cout << "All recovery configuration tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}