#pragma once

#include <memory>
#include <string>
#include <vector>
#include "StorageDeviceInfo.h"
#include "StorageExceptions.h"
#include "Version.h"

namespace recoverysuite {
namespace storage {

/**
 * @brief Reporter for storage analysis results
 *
 * This class formats and presents storage analysis results to the user
 * and other subsystems.
 */
class StorageReporter {
public:
    StorageReporter();
    ~StorageReporter();

    // Disable copy/move
    StorageReporter(const StorageReporter&) = delete;
    StorageReporter& operator=(const StorageReporter&) = delete;

    /**
     * @brief Generate a comprehensive storage analysis report
     *
     * @param deviceInfo Basic storage device information
     * @param trimStatus TRIM status assessment
     * @param wearLeveling Wear leveling quality assessment
     * @param gcAggressiveness Garbage collection aggressiveness assessment
     * @param healthData SMART/health data
     * @return StorageAnalysisReport containing the complete analysis
     */
    StorageAnalysisReport generateReport(
        const StorageDeviceInfo& deviceInfo,
        TRIMStatus trimStatus,
        WearLevelingQuality wearLeveling,
        GarbageCollectionAggressiveness gcAggressiveness,
        const StorageHealthData& healthData);

    /**
     * @brief Generate a human-readable health summary
     *
     * @param deviceInfo Basic storage device information
     * @param trimStatus TRIM status assessment
     * @param wearLeveling Wear leveling quality assessment
     * @param gcAggressiveness Garbage collection aggressiveness assessment
     * @param healthData SMART/health data
     * @return Human-readable health summary string
     */
    std::string generateHealthSummary(
        const StorageDeviceInfo& deviceInfo,
        TRIMStatus trimStatus,
        WearLevelingQuality wearLeveling,
        GarbageCollectionAggressiveness gcAggressiveness,
        const StorageHealthData& healthData);

private:
    class StorageReporterImpl;
    std::unique_ptr<StorageReporterImpl> pImpl;

    /**
     * @brief Calculate a recoverability estimate based on storage health indicators
     *
     * @param trimStatus TRIM status assessment
     * @param wearLeveling Wear leveling quality assessment
     * @param gcAggressiveness Garbage collection aggressiveness assessment
     * @param healthData SMART/health data
     * @return Recoverability estimate as a percentage (0-100)
     */
    double calculateRecoverabilityEstimate(
        TRIMStatus trimStatus,
        WearLevelingQuality wearLeveling,
        GarbageCollectionAggressiveness gcAggressiveness,
        const StorageHealthData& healthData);
};

} // namespace storage
} // namespace recoverysuite