#pragma once

#include <memory>
#include <string>
#include "StorageDeviceInfo.h"
#include "StorageExceptions.h"
#include "Version.h"

namespace recoverysuite {
namespace storage {

/**
 * @brief Analyst for wear leveling quality assessment
 *
 * This class analyzes storage device behavior to assess wear leveling quality.
 */
class WearLevelingAnalyst {
public:
    WearLevelingAnalyst();
    ~WearLevelingAnalyst();

    // Disable copy/move
    WearLevelingAnalyst(const WearLevelingAnalyst&) = delete;
    WearLevelingAnalyst& operator=(const WearLevelingAnalyst&) = delete;

    /**
     * @brief Analyze wear leveling quality for a storage device
     *
     * @param diskNumber The physical disk number to analyze
     * @return WearLevelingQuality indicating wear leveling quality
     * @throws StorageException if analysis fails
     */
    WearLevelingQuality analyze(uint64_t diskNumber);

private:
    class WearLevelingAnalystImpl;
    std::unique_ptr<WearLevelingAnalystImpl> pImpl;
};

} // namespace storage
} // namespace recoverysuite