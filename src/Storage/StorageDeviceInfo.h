#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include "../Core/Version.h"

namespace recoverysuite {
namespace storage {

/**
 * @brief Enum for storage interface types
 */
enum class StorageInterfaceType {
    UNKNOWN,
    SATA,
    NVMe,
    USB_SATA,
    USB_NVMe,
    PCIe,
    MMC,
    UFS
};

/**
 * @brief Enum for TRIM status
 */
enum class TRIMStatus {
    UNKNOWN,
    DISABLED,
    ENABLED
};

/**
 * @brief Enum for wear leveling quality assessment
 */
enum class WearLevelingQuality {
    UNKNOWN,
    POOR,
    FAIR,
    GOOD,
    EXCELLENT
};

/**
 * @brief Enum for garbage collection aggressiveness
 */
enum class GarbageCollectionAggressiveness {
    UNKNOWN,
    LOW,
    MEDIUM,
    HIGH
};

/**
 * @brief Structure to hold basic storage device information
 */
struct StorageDeviceInfo {
    std::string device_path;          // Device path (e.g., "\\\\.\\PhysicalDrive0" on Windows)
    StorageInterfaceType interface_type;
    std::string vendor;
    std::string model;
    std::string revision;
    std::string serial_number;
    uint64_t user_capacity_bytes;     // User accessible capacity in bytes
    uint32_t sector_size_bytes;       // Usually 512 or 4096
    std::string form_factor;          // M.2, 2.5", U.2, etc.
    bool is_removable;                // USB vs internal
    uint64_t block_size_bytes;        // Internal block size (for NAND-based devices)
    uint64_t nand_capacity_bytes;     // Estimated raw NAND capacity (if available)
};

/**
 * @brief Structure to hold SMART/health data
 */
struct StorageHealthData {
    uint8_t critical_warning;
    uint16_t temperature_kelvin;      // Temperature in Kelvin
    uint8_t available_spare_percent;  // Available spare as percentage (0-100)
    uint8_t spare_threshold_percent;  // Spare threshold percentage
    uint8_t percentage_used;          // Percentage used (0-100)
    uint64_t data_read_gb;            // Data read in GB
    uint64_t data_written_gb;         // Data written in GB
    uint64_t host_read_commands;
    uint64_t host_write_commands;
    uint32_t power_cycles;
    uint32_t power_on_hours;
    uint32_t unsafe_shutdowns;
    uint32_t media_errors;
    uint32_t error_log_entries;
    // Additional vendor-specific attributes can be added by derived classes
};

/**
 * @brief Structure to hold comprehensive storage analysis report
 */
struct StorageAnalysisReport {
    StorageDeviceInfo device_info;
    TRIMStatus trim_status;
    double trim_confidence;           // 0.0 to 1.0
    GarbageCollectionAggressiveness gc_aggressiveness;
    double gc_confidence;             // 0.0 to 1.0
    WearLevelingQuality wear_leveling;
    double wear_confidence;           // 0.0 to 1.0
    double estimated_overprovisioning_pct; // 0-100+
    double write_amplification_factor;
    StorageHealthData health_data;
    std::string vendor_specific_notes;
    std::vector<std::string> alerts;  // Critical warnings, etc.
    uint64_t analysis_timestamp;      // Unix timestamp
    std::string overall_health_summary; // Human readable summary
    double recoverability_estimate;   // Estimated recoverability percentage (0-100)
};

} // namespace storage
} // namespace recoverysuite