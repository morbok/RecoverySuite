#ifndef RECOVERYSUITE_FILESYSTEM_SIMPLE_FILESYSTEM_REGISTRY_H
#define RECOVERYSUITE_FILESYSTEM_SIMPLE_FILESYSTEM_REGISTRY_H

#include <memory>
#include <string>
#include <vector>
#include "FilesystemRegistry.h"
#include "FilesystemDriver.h"

namespace recoverysuite {
namespace filesystem {

// Simple implementation of FilesystemRegistry
class SimpleFilesystemRegistry : public FilesystemRegistry {
public:
    SimpleFilesystemRegistry() = default;
    ~SimpleFilesystemRegistry() override = default;

    // FilesystemRegistry interface
    void registerDriver(std::shared_ptr<FilesystemDriver> driver) override;
    void unregisterDriver(std::shared_ptr<FilesystemDriver> driver) override;
    std::shared_ptr<FilesystemDriver> findDriver(FilesystemType type) const override;
    std::shared_ptr<FilesystemDriver> findDriver(const uint8_t* bootSectorData, uint32_t bootSectorSize) const override;
    std::vector<std::shared_ptr<FilesystemDriver>> getAllDrivers() const override;
    void clearDrivers() override;

private:
    std::vector<std::shared_ptr<FilesystemDriver>> drivers_;
};

} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_SIMPLE_FILESYSTEM_REGISTRY_H
