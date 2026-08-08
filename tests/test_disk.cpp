#include <iostream>
#include "Disk/DiskManager.h"
#include "Disk/DiskEnumerator.h"
#include "Disk/DeviceInformation.h"
#include "Core/Version.h"
#include <memory>
#include <vector>
#include <stdexcept>

// Mock disk enumerator for testing
namespace recoverysuite {
namespace platform {

class MockDiskEnumerator : public virtual IDiskEnumerator {
public:
    ~MockDiskEnumerator() override = default;

    std::vector<recoverysuite::disk::DeviceInformation> enumerateDisks() override {
        // Return empty vector for testing
        return {};
    }

    recoverysuite::disk::DeviceInformation getDiskInfo(uint64_t diskNumber) override {
        // Return mock device info
        recoverysuite::disk::DeviceInformation info;
        info.modelNumber = "Mock Disk";
        info.serialNumber = "MOCK123";
        info.firmwareRevision = "1.0";
        info.bytesPerSector = 512;
        info.totalSectors = 1000;
        info.busType = recoverysuite::disk::BusType::SATA; // Set to SATA as an example
        return info;
    }

    uint64_t getDiskCount() override {
        return 0;
    }
};

} // namespace platform
} // namespace recoverysuite

// Simple test to verify the disk module interface
int main() {
    std::cout << "Testing RecoverySuite Disk Module interface..." << std::endl;

    try {
        // Test creating a disk enumerator (use mock for testing)
        auto enumerator = std::make_unique<recoverysuite::platform::MockDiskEnumerator>();
        std::cout << "��������������������������������������������������������������✓ DiskEnumerator created successfully" << std::endl;

        // Test creating a disk manager
        recoverysuite::disk::DiskManager diskManager;
        std::cout << "��������������������������������������������������������������✓ DiskManager created successfully" << std::endl;

        // Test accessing version information
        std::cout << "��������������������������������������������������������������✓ RecoverySuite Version: "
                  << RECOVERYSUITE_VERSION_MAJOR << "."
                  << RECOVERYSUITE_VERSION_MINOR << "."
                  << RECOVERYSUITE_VERSION_PATCH << std::endl;

        std::cout << "Disk module interface test completed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}