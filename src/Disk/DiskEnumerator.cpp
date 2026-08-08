#include "DiskEnumerator.h"
#include <stdexcept>
#include <memory>
#include "DiskExceptions.h"

#include "Version.h"

#if defined(_WIN32) || defined(_WIN64)
#include "../Platform/Windows/Disk/WindowsDiskEnumerator.h"
#elif defined(__linux__)
#include "../Platform/Linux/Disk/LinuxDiskEnumerator.h"
#endif

namespace recoverysuite {
namespace disk {

std::unique_ptr<platform::IDiskEnumerator> createDiskEnumerator() {
#if defined(_WIN32) || defined(_WIN64)
    return std::make_unique<recoverysuite::platform::windows::WindowsDiskEnumerator>();
#elif defined(__linux__)
    return std::make_unique<recoverysuite::platform::linux::LinuxDiskEnumerator>();
#else
    // For other platforms, throw an exception indicating not implemented
    // In a real implementation, we would have platform-specific implementations
    throw DiskException("Disk enumerator not implemented for this platform",
                        0x80000001); // Using a placeholder error code
#endif
}

} // namespace disk
} // namespace recoverysuite