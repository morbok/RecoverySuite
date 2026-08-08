#ifndef RECOVERYSUITE_FILESYSTEM_FILESYSTEM_MANAGER_H
#define RECOVERYSUITE_FILESYSTEM_FILESYSTEM_MANAGER_H

#include <memory>
#include <string>
#include <vector>
#include "FilesystemTypes.h"
#include "Filesystem.h"
#include "FilesystemRegistry.h"

namespace recoverysuite {
namespace filesystem {

// FilesystemManager provides a high-level interface for filesystem operations.
class FilesystemManager {
public:
    explicit FilesystemManager(std::shared_ptr<FilesystemRegistry> registry);
    ~FilesystemManager() = default;

    // Detect filesystem type from boot sector data
    // @param bootSectorData: pointer to boot sector data
    // @param bootSectorSize: size of boot sector data in bytes
    // @return detected filesystem type and confidence level
    std::pair<FilesystemType, double> detectFilesystem(const uint8_t* bootSectorData, uint32_t bootSectorSize) const;

    // Create a filesystem instance for the given type
    // @param type: the filesystem type to create
    // @return a filesystem instance, or nullptr if creation failed
    std::shared_ptr<Filesystem> createFilesystem(FilesystemType type) const;

    // Create a filesystem instance by probing boot sector data
    // @param bootSectorData: pointer to boot sector data
    // @param bootSectorSize: size of boot sector data in bytes
    // @return a filesystem instance, or nullptr if detection/creation failed
    std::shared_ptr<Filesystem> createFilesystemFromBootSector(const uint8_t* bootSectorData, uint32_t bootSectorSize) const;

    // Get all supported filesystem types
    std::vector<FilesystemType> getSupportedTypes() const;

    // Get the registry being used
    std::shared_ptr<FilesystemRegistry> getRegistry() const;

private:
    std::shared_ptr<FilesystemRegistry> registry_;
};

} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FILESYSTEM_MANAGER_H
