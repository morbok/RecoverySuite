#include "DiskManager.h"
#include "DiskEnumerator.h"
#include "../Platform/IDiskEnumerator.h"
#include <stdexcept>
#include "Version.h"

namespace recoverysuite {
namespace disk {

class DiskManager::DiskManagerImpl {
public:
    DiskManagerImpl() {
        // In a real implementation, this might initialize platform-specific resources
        m_enumerator = createDiskEnumerator();
    }

    ~DiskManagerImpl() = default;

    std::vector<DeviceInformation> enumerateDisks() {
        if (!m_enumerator) {
            throw DiskException("Disk enumerator not initialized");
        }
        return m_enumerator->enumerateDisks();
    }

    DeviceInformation getDiskInfo(uint64_t diskNumber) {
        if (!m_enumerator) {
            throw DiskException("Disk enumerator not initialized");
        }
        return m_enumerator->getDiskInfo(diskNumber);
    }

    uint64_t getDiskCount() {
        if (!m_enumerator) {
            throw DiskException("Disk enumerator not initialized");
        }
        return m_enumerator->getDiskCount();
    }

    std::unique_ptr<PhysicalDisk> openDisk(uint64_t diskNumber, bool readOnly) {
        // In a real implementation, we might validate the disk number first
        return std::make_unique<PhysicalDisk>(diskNumber, readOnly);
    }

private:
    std::unique_ptr<recoverysuite::platform::IDiskEnumerator> m_enumerator;
};

DiskManager::DiskManager()
    : pImpl(std::make_unique<DiskManagerImpl>()) {}

DiskManager::~DiskManager() = default;

std::vector<DeviceInformation> DiskManager::enumerateDisks() {
    return pImpl->enumerateDisks();
}

DeviceInformation DiskManager::getDiskInfo(uint64_t diskNumber) {
    return pImpl->getDiskInfo(diskNumber);
}

std::unique_ptr<PhysicalDisk> DiskManager::openDisk(uint64_t diskNumber, bool readOnly) {
    return pImpl->openDisk(diskNumber, readOnly);
}

uint64_t DiskManager::getDiskCount() {
    return pImpl->getDiskCount();
}

} // namespace disk
} // namespace recoverysuite