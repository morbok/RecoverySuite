#ifndef RECOVERYSUITE_FILESYSTEM_FILESYSTEM_H
#define RECOVERYSUITE_FILESYSTEM_FILESYSTEM_H

#include <memory>
#include <string>
#include "FilesystemTypes.h"
#include "FilesystemExceptions.h"

namespace recoverysuite {
namespace filesystem {

// Forward declarations
class FilesystemReader;
class FilesystemAnalyzer;
class FilesystemMetadata;
class FilesystemVolume;

// Base class for all filesystems
class Filesystem {
public:
    virtual ~Filesystem() = default;

    // Get the type of this filesystem
    virtual FilesystemType getType() const = 0;

    // Get the name of this filesystem (e.g., "NTFS")
    virtual std::string getName() const = 0;

    // Get the version of this filesystem (if applicable)
    virtual std::string getVersion() const = 0;

    // Check if the filesystem supports a given capability
    virtual bool supportsCapability(FilesystemCapability capability) const = 0;

    // Get the set of capabilities supported by this filesystem
    virtual FilesystemCapabilities getCapabilities() const = 0;

    // Open a reader for this filesystem
    virtual std::unique_ptr<FilesystemReader> createReader() const = 0;

    // Open an analyzer for this filesystem
    virtual std::unique_ptr<FilesystemAnalyzer> createAnalyzer() const = 0;

    // Get metadata about this filesystem
    virtual std::unique_ptr<FilesystemMetadata> getMetadata() const = 0;

    // Get a volume representation of this filesystem
    virtual std::unique_ptr<FilesystemVolume> getVolume() const = 0;
};

} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FILESYSTEM_H
