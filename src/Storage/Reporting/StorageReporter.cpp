#include "StorageReporter.h"
#include <stdexcept>
#include <memory>
#include <sstream>
#include <iomanip>
#include <chrono>

#include "StorageUtils.h"

namespace recoverysuite {
namespace storage {

class StorageReporter::StorageReporterImpl {
public:
    StorageReporterImpl()
        : storageUtils(std::make_unique<StorageUtils>()) {}

    std::unique_ptr<StorageUtils> storageUtils;
};

StorageReporter::StorageReporter() : pImpl(std::make_unique<StorageReporterImpl>()) {}

StorageReporter::~StorageReporter() = default;

StorageAnalysisReport StorageReporter::generateReport(
    const StorageDeviceInfo& deviceInfo,
    TRIMStatus trimStatus,
    WearLevelingQuality wearLeveling,
    GarbageCollectionAggressiveness gcAggressiveness,
    const StorageHealthData& healthData) {

    StorageAnalysisReport report;
    report.device_info = deviceInfo;
    report.trim_status = trimStatus;
    report.trim_confidence = 0.0; // Would be calculated in real implementation
    report.gc_aggressiveness = gcAggressiveness;
    report.gc_confidence = 0.0; // Would be calculated in real implementation
    report.wear_leveling = wearLeveling;
    report.wear_confidence = 0.0; // Would be calculated in real implementation
    report.estimated_overprovisioning_pct = 0.0; // Would be calculated in real implementation
    report.write_amplification_factor = 1.0; // Would be calculated in real implementation
    report.health_data = healthData;
    report.vendor_specific_notes = ""; // Would be populated in real implementation
    report.alerts = {}; // Would be populated in real implementation

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    report.analysis_timestamp = std::chrono::system_clock::to_time_t(now);

    // Generate health summary
    report.overall_health_summary = generateHealthSummary(
        deviceInfo, trimStatus, wearLeveling, gcAggressiveness, healthData);

    // Calculate recoverability estimate (simplified)
    report.recoverability_estimate = calculateRecoverabilityEstimate(
        trimStatus, wearLeveling, gcAggressiveness, healthData);

    return report;
}

std::string StorageReporter::generateHealthSummary(
    const StorageDeviceInfo& deviceInfo,
    TRIMStatus trimStatus,
    WearLevelingQuality wearLeveling,
    GarbageCollectionAggressiveness gcAggressiveness,
    const StorageHealthData& healthData) {

    std::ostringstream oss;

    oss << "Storage Device: " << deviceInfo.model << " (" << deviceInfo.vendor << ")\n";
    oss << "Interface: ";

    switch (deviceInfo.interface_type) {
        case StorageInterfaceType::UNKNOWN: oss << "Unknown"; break;
        case StorageInterfaceType::SATA: oss << "SATA"; break;
        case StorageInterfaceType::NVMe: oss << "NVMe"; break;
        case StorageInterfaceType::USB_SATA: oss << "USB SATA"; break;
        case StorageInterfaceType::USB_NVMe: oss << "USB NVMe"; break;
        case StorageInterfaceType::PCIe: oss << "PCIe"; break;
        case StorageInterfaceType::MMC: oss << "MMC"; break;
        case StorageInterfaceType::UFS: oss << "UFS"; break;
    }

    oss << "\nCapacity: " << (deviceInfo.user_capacity_bytes / (1024.0*1024.0*1024.0))
        << " GB (" << deviceInfo.user_capacity_bytes << " bytes)\n";

    oss << "TRIM Status: ";
    switch (trimStatus) {
        case TRIMStatus::UNKNOWN: oss << "Unknown"; break;
        case TRIMStatus::DISABLED: oss << "Disabled"; break;
        case TRIMStatus::ENABLED: oss << "Enabled"; break;
    }
    oss << "\n";

    oss << "Wear Leveling: ";
    switch (wearLeveling) {
        case WearLevelingQuality::UNKNOWN: oss << "Unknown"; break;
        case WearLevelingQuality::POOR: oss << "Poor"; break;
        case WearLevelingQuality::FAIR: oss << "Fair"; break;
        case WearLevelingQuality::GOOD: oss << "Good"; break;
        case WearLevelingQuality::EXCELLENT: oss << "Excellent"; break;
    }
    oss << "\n";

    oss << "Garbage Collection: ";
    switch (gcAggressiveness) {
        case GarbageCollectionAggressiveness::UNKNOWN: oss << "Unknown"; break;
        case GarbageCollectionAggressiveness::LOW: oss << "Low"; break;
        case GarbageCollectionAggressiveness::MEDIUM: oss << "Medium"; break;
        case GarbageCollectionAggressiveness::HIGH: oss << "High"; break;
    }
    oss << "\n";

    if (healthData.temperature_kelvin > 0) {
        double tempC = healthData.temperature_kelvin - 273.15;
        oss << "Temperature: " << std::fixed << std::setprecision(1) << tempC << "°C\n";
    }

    if (healthData.percentage_used > 0) {
        oss << "Usage: " << static_cast<int>(healthData.percentage_used) << "%\n";
    }

    return oss.str();
}

double StorageReporter::calculateRecoverabilityEstimate(
    TRIMStatus trimStatus,
    WearLevelingQuality wearLeveling,
    GarbageCollectionAggressiveness gcAggressiveness,
    const StorageHealthData& healthData) {

    // Simplified recoverability estimation
    // In a real implementation, this would be much more sophisticated

    double baseScore = 100.0; // Start with perfect score

    // Penalize for enabled TRIM (makes recovery harder)
    if (trimStatus == TRIMStatus::ENABLED) {
        baseScore -= 30.0;
    } else if (trimStatus == TRIMStatus::UNKNOWN) {
        baseScore -= 15.0; // Unknown TRIM status is somewhat concerning
    }

    // Penalize for poor wear leveling
    switch (wearLeveling) {
        case WearLevelingQuality::POOR: baseScore -= 25.0; break;
        case WearLevelingQuality::FAIR: baseScore -= 10.0; break;
        case WearLevelingQuality::GOOD: baseScore -= 5.0; break;
        case WearLevelingQuality::EXCELLENT: baseScore -= 0.0; break;
        case WearLevelingQuality::UNKNOWN: baseScore -= 15.0; break;
    }

    // Penalize for aggressive garbage collection
    switch (gcAggressiveness) {
        case GarbageCollectionAggressiveness::HIGH: baseScore -= 20.0; break;
        case GarbageCollectionAggressiveness::MEDIUM: baseScore -= 10.0; break;
        case GarbageCollectionAggressiveness::LOW: baseScore -= 5.0; break;
        case GarbageCollectionAggressiveness::UNKNOWN: baseScore -= 10.0; break;
    }

    // Penalize for high usage (less free space for wear leveling)
    if (healthData.percentage_used > 80) {
        baseScore -= 15.0;
    } else if (healthData.percentage_used > 60) {
        baseScore -= 10.0;
    } else if (healthData.percentage_used > 40) {
        baseScore -= 5.0;
    }

    // Ensure score doesn't go below 0 or above 100
    if (baseScore < 0.0) baseScore = 0.0;
    if (baseScore > 100.0) baseScore = 100.0;

    return baseScore;
}

} // namespace storage
} // namespace recoverysuite