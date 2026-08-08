#ifndef RECOVERYSUITE_FILESYSTEM_FILESYSTEM_REGISTRY_H
#define RECOVERYSUITE_FILESYSTEM_FILESYSTEM_REGISTRY_H

#include <memory>
#include <string>
#include <vector>
#include "FilesystemTypes.h"
#include "FilesystemDriver.h"

namespace recoverysuite {
namespace filesystem {

// FilesystemRegistry manages the registration and lookup of filesystem drivers.
class FilesystemRegistry {
public:
    virtual ~FilesystemRegistry() = default;

    // Register a filesystem driver
    // @param driver: the driver to register
    virtual void registerDriver(std::shared_ptr<FilesystemDriver> driver) = 0;

    // Unregister a filesystem driver
    // @param driver: the driver to unregister
    virtual void unregisterDriver(std::shared_ptr<FilesystemDriver> driver) = 0;

    // Find a driver that can handle the given filesystem type
    // @param type: the filesystem type to look for
    // @return a driver that can handle the type, or nullptr if none found
    virtual std::shared_ptr<FilesystemDriver> findDriver(FilesystemType type) const = 0;

    // Find a driver that can handle the given boot sector or partition data
    // @param bootSectorData: pointer to boot sector data
    // @param bootSectorSize: size of boot sector data in bytes
    // @return a driver that can handle the data, or nullptr if none found
    virtual std::shared_ptr<FilesystemDriver> findDriver(const uint8_t* bootSectorData, uint32_t bootSectorSize) const = 0;

    // Get all registered drivers
    virtual std::vector<std::shared_ptr<FilesystemDriver>> getAllDrivers() const = 0;

    // Clear all registered drivers
    virtual void clearDrivers() = 0;
};

} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FILESYSTEM_REGISTRY_H
