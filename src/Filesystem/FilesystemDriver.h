#ifndef RECOVERYSUITE_FILESYSTEM_FILESYSTEM_DRIVER_H
#define RECOVERYSUITE_FILESYSTEM_FILESYSTEM_DRIVER_H

#include <memory>
#include <string>
#include <vector>
#include "FilesystemTypes.h"
#include "Filesystem.h"
#include "FilesystemReader.h"

namespace recoverysuite {
namespace filesystem {

// Forward declaration
class FilesystemReader;

// FilesystemDriver is responsible for identifying a filesystem type and creating a filesystem instance.
class FilesystemDriver {
public:
    virtual ~FilesystemDriver() = default;

    // Get the filesystem type this driver handles
    virtual FilesystemType getFilesystemType() const = 0;

    // Get a human-readable name for this driver
    virtual std::string getName() const = 0;

    // Check if the given boot sector data matches this filesystem type
    // @param bootSectorData: pointer to boot sector data
    // @param bootSectorSize: size of boot sector data in bytes
    // @return confidence level (0.0 to 1.0) of match, 0.0 means no match
    virtual double probe(const uint8_t* bootSectorData, uint32_t bootSectorSize) const = 0;

    // Create a filesystem instance for the given type
    // @return a filesystem instance, or nullptr if creation failed
    virtual std::shared_ptr<Filesystem> createFilesystem() const = 0;

    // Get the list of filesystem types this driver can handle
    // @return vector of supported filesystem types
    virtual std::vector<FilesystemType> getSupportedTypes() const = 0;
};

} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FILESYSTEM_DRIVER_H
