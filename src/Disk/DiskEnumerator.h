#pragma once

#include <vector>
#include <memory>
#include "DeviceInformation.h"
#include "DiskExceptions.h"
#include "../Core/Version.h"
#include "../Platform/IDiskEnumerator.h"

namespace recoverysuite {
namespace disk {

/**
 * @brief Interface for enumerating disk devices
 *
 * This interface allows for platform-independent disk enumeration.
 * Platform-specific implementations will be provided in Platform-specific directories.
 */
class DiskEnumerator : public virtual platform::IDiskEnumerator {
public:
    ~DiskEnumerator() override = default;
};

/**
 * @brief Factory function to create a platform-specific disk enumerator
 *
 * @return Unique pointer to a platform::IDiskEnumerator implementation
 * @throws DiskException if creation fails
 */
std::unique_ptr<recoverysuite::platform::IDiskEnumerator> createDiskEnumerator();

} // namespace disk
} // namespace recoverysuite