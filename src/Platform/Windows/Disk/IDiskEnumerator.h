#pragma once

#include <vector>
#include <RecoverySuite/Core/Version.h>
#include "../IDiskEnumerator.h"

namespace recoverysuite {
namespace platform {
namespace windows {

/**
 * @brief Windows-specific disk enumerator interface
 *
 * This interface extends the base IDiskEnumerator and provides
 * Windows-specific functionality if needed in the future.
 */
class IDiskEnumerator : public recoverysuite::disk::DiskEnumerator {
public:
    ~IDiskEnumerator() override = default;
};

} // namespace windows
} // namespace platform
} // namespace recoverysuite