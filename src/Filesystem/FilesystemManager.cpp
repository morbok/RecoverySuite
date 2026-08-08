#include "FilesystemManager.h"
#include <algorithm>

namespace recoverysuite {
namespace filesystem {

FilesystemManager::FilesystemManager(std::shared_ptr<FilesystemRegistry> registry)
    : registry_(std::move(registry)) {}

std::pair<FilesystemType, double> FilesystemManager::detectFilesystem(const uint8_t* bootSectorData, uint32_t bootSectorSize) const {
    if (!registry_) {
        return {FilesystemType::UNKNOWN, 0.0};
    }
    
    // Ask the registry to find a driver that can handle this boot sector
    auto driver = registry_->findDriver(bootSectorData, bootSectorSize);
    if (!driver) {
        return {FilesystemType::UNKNOWN, 0.0};
    }
    
    // Get the confidence level from the driver's probe method
    double confidence = driver->probe(bootSectorData, bootSectorSize);
    FilesystemType type = driver->getFilesystemType();
    
    return {type, confidence};
}

std::shared_ptr<Filesystem> FilesystemManager::createFilesystem(FilesystemType type) const {
    if (!registry_) {
        return nullptr;
    }
    
    // Find a driver that supports this filesystem type
    auto driver = registry_->findDriver(type);
    if (!driver) {
        return nullptr;
    }
    
    // Create the filesystem using the driver
    return driver->createFilesystem();
}

std::shared_ptr<Filesystem> FilesystemManager::createFilesystemFromBootSector(const uint8_t* bootSectorData, uint32_t bootSectorSize) const {
    auto detection = detectFilesystem(bootSectorData, bootSectorSize);
    FilesystemType type = detection.first;
    double confidence = detection.second;
    
    // Only proceed if we have reasonable confidence
    if (confidence < 0.5) {
        return nullptr;
    }
    
    return createFilesystem(type);
}

std::vector<FilesystemType> FilesystemManager::getSupportedTypes() const {
    if (!registry_) {
        return {};
    }
    
    std::vector<FilesystemType> allTypes;
    auto drivers = registry_->getAllDrivers();
    
    for (const auto& driver : drivers) {
        auto supported = driver->getSupportedTypes();
        allTypes.insert(allTypes.end(), supported.begin(), supported.end());
    }
    
    // Remove duplicates
    std::sort(allTypes.begin(), allTypes.end());
    auto last = std::unique(allTypes.begin(), allTypes.end());
    allTypes.erase(last, allTypes.end());
    
    return allTypes;
}

std::shared_ptr<FilesystemRegistry> FilesystemManager::getRegistry() const {
    return registry_;
}

} // namespace filesystem
} // namespace recoverysuite
