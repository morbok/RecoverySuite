#pragma once

#include <string>
#include <vector>
#include "../StorageAccess.h"

namespace recoverysuite {
namespace storage {

/**
 * @brief Windows-specific storage access implementation
 *
 * This class provides Windows-specific storage access functionality
 * using observed disk metadata. Unmeasured diagnostics remain unavailable.
 */
class WindowsStorageAccess : public StorageAccess {
public:
    WindowsStorageAccess();
    ~WindowsStorageAccess() override;

    // Disable copy/move
    WindowsStorageAccess(const WindowsStorageAccess&) = delete;
    WindowsStorageAccess& operator=(const WindowsStorageAccess&) = delete;

    // Override base class methods with Windows-specific implementations
    StorageDeviceInfo getDeviceInfo(uint64_t diskNumber) override;
    StorageHealthData getHealthData(uint64_t diskNumber) override;
    TRIMStatus getTrimStatus(uint64_t diskNumber) override;
    WearLevelingQuality getWearLevelingQuality(uint64_t diskNumber) override;
    GarbageCollectionAggressiveness getGcAggressiveness(uint64_t diskNumber) override;
    uint32_t sendVendorCommand(
        uint64_t diskNumber,
        const std::vector<uint8_t>& command,
        std::vector<uint8_t>& buffer) override;

private:
    class WindowsStorageAccessImpl;
    std::unique_ptr<WindowsStorageAccessImpl> pImpl;
};

} // namespace storage
} // namespace recoverysuite