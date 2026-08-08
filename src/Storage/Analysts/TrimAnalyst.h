#pragma once

#include <memory>
#include <string>
#include "StorageDeviceInfo.h"
#include "StorageExceptions.h"
#include "Version.h"

namespace recoverysuite {
namespace storage {

/**
 * @brief Analyst for TRIM status detection
 *
 * This class analyzes storage device behavior to determine if TRIM is enabled.
 */
class TrimAnalyst {
public:
    TrimAnalyst();
    ~TrimAnalyst();

    // Disable copy/move
    TrimAnalyst(const TrimAnalyst&) = delete;
    TrimAnalyst& operator=(const TrimAnalyst&) = delete;

    /**
     * @brief Analyze TRIM status for a storage device
     *
     * @param diskNumber The physical disk number to analyze
     * @return TRIMStatus indicating TRIM status
     * @throws StorageException if analysis fails
     */
    TRIMStatus analyze(uint64_t diskNumber);

private:
    class TrimAnalystImpl;
    std::unique_ptr<TrimAnalystImpl> pImpl;
};

} // namespace storage
} // namespace recoverysuite