#include "SimpleFilesystemRegistry.h"
#include <algorithm>

namespace recoverysuite {
namespace filesystem {

void SimpleFilesystemRegistry::registerDriver(std::shared_ptr<FilesystemDriver> driver) {
    if (driver) {
        // Avoid duplicate registrations
        auto it = std::find_if(drivers_.begin(), drivers_.end(),
            [&](const std::shared_ptr<FilesystemDriver>& d) {
                return d.get() == driver.get();
            });
        
        if (it == drivers_.end()) {
            drivers_.push_back(driver);
        }
    }
}

void SimpleFilesystemRegistry::unregisterDriver(std::shared_ptr<FilesystemDriver> driver) {
    if (driver) {
        drivers_.erase(
            std::remove_if(drivers_.begin(), drivers_.end(),
                [&](const std::shared_ptr<FilesystemDriver>& d) {
                    return d.get() == driver.get();
                }),
            drivers_.end());
    }
}

std::shared_ptr<FilesystemDriver> SimpleFilesystemRegistry::findDriver(FilesystemType type) const {
    auto it = std::find_if(drivers_.begin(), drivers_.end(),
        [&](const std::shared_ptr<FilesystemDriver>& driver) {
            if (driver) {
                auto supportedTypes = driver->getSupportedTypes();
                return std::find(supportedTypes.begin(), supportedTypes.end(), type) != supportedTypes.end();
            }
            return false;
        });
    
    return (it != drivers_.end()) ? *it : nullptr;
}

std::shared_ptr<FilesystemDriver> SimpleFilesystemRegistry::findDriver(const uint8_t* bootSectorData, uint32_t bootSectorSize) const {
    std::shared_ptr<FilesystemDriver> bestDriver;
    double bestConfidence = 0.0;
    
    for (const auto& driver : drivers_) {
        if (driver) {
            double confidence = driver->probe(bootSectorData, bootSectorSize);
            if (confidence > bestConfidence) {
                bestConfidence = confidence;
                bestDriver = driver;
            }
        }
    }
    
    // Only return a driver if we have reasonable confidence
    if (bestConfidence > 0.0) {
        return bestDriver;
    }
    
    return nullptr;
}

std::vector<std::shared_ptr<FilesystemDriver>> SimpleFilesystemRegistry::getAllDrivers() const {
    return drivers_;
}

void SimpleFilesystemRegistry::clearDrivers() {
    drivers_.clear();
}

} // namespace filesystem
} // namespace recoverysuite
