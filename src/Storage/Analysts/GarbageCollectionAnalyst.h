#pragma once

#include <memory>
#include <string>
#include "StorageDeviceInfo.h"
#include "StorageExceptions.h"
#include "Version.h"

namespace recoverysuite {
namespace storage {

/**
 * @brief Analyst for garbage collection aggressiveness assessment
 *
 * This class analyzes storage device behavior to assess garbage collection aggressiveness.
 */
class GarbageCollectionAnalyst {
public:
    GarbageCollectionAnalyst();
    ~GarbageCollectionAnalyst();

    // Disable copy/move
    GarbageCollectionAnalyst(const GarbageCollectionAnalyst&) = delete;
    GarbageCollectionAnalyst& operator=(const GarbageCollectionAnalyst&) = delete;

    /**
     * @brief Analyze garbage collection aggressiveness for a storage device
     *
     * @param diskNumber The physical disk number to analyze
     * @return GarbageCollectionAggressiveness indicating GC aggressiveness
     * @throws StorageException if analysis fails
     */
    GarbageCollectionAggressiveness analyze(uint64_t diskNumber);

private:
    class GarbageCollectionAnalystImpl;
    std::unique_ptr<GarbageCollectionAnalystImpl> pImpl;
};

} // namespace storage
} // namespace recoverysuite