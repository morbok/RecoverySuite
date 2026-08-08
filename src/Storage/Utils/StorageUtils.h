#pragma once

#include <string>
#include <vector>
#include "StorageDeviceInfo.h"
#include "StorageExceptions.h"
#include "../../Core/Version.h"

namespace recoverysuite {
namespace storage {

/**
 * @brief Utility functions for storage analysis
 *
 * This class provides utility functions used throughout the storage subsystem.
 */
class StorageUtils {
public:
    StorageUtils();
    ~StorageUtils();

    // Disable copy/move
    StorageUtils(const StorageUtils&) = delete;
    StorageUtils& operator=(const StorageUtils&) = delete;

    /**
     * @brief Convert bytes to human-readable format
     *
     * @param bytes Number of bytes
     * @return Human-readable string (e.g., "1.5 GB")
     */
    static std::string formatBytes(uint64_t bytes);

    /**
     * @brief Convert temperature from Kelvin to Celsius
     *
     * @param kelvin Temperature in Kelvin
     * @return Temperature in Celsius
     */
    static double kelvinToCelsius(uint16_t kelvin);

    /**
     * @brief Convert temperature from Celsius to Kelvin
     *
     * @param celsius Temperature in Celsius
     * @return Temperature in Kelvin
     */
    static uint16_t celsiusToKelvin(double celsius);

    /**
     * @brief Parse vendor and model from device identification string
     *
     * @param identification Raw device identification string
     * @param vendor Output vendor string
     * @param model Output model string
     */
    static void parseVendorModel(const std::string& identification,
                                std::string& vendor,
                                std::string& model);

    /**
     * @brief Calculate write amplification factor
     *
     * @param hostWrites Host write operations in GB
     * @param nandWrites Estimated NAND write operations in GB
     * @return Write amplification factor (hostWrites / nandWrites)
     */
    static double calculateWriteAmplification(double hostWrites, double nandWrites);

    /**
     * @brief Estimate over-provisioning percentage
     *
     * @param userCapacity User accessible capacity in bytes
     * @param rawNandCapacity Estimated raw NAND capacity in bytes
     * @return Over-provisioning percentage
     */
    static double estimateOverprovisioningPct(uint64_t userCapacity, uint64_t rawNandCapacity);

    /**
     * @brief Check if a storage device is likely an SSD based on characteristics
     *
     * @param deviceInfo Storage device information
     * @return True if likely SSD, false if likely HDD
     */
    static bool isLikelySsd(const StorageDeviceInfo& deviceInfo);
};

} // namespace storage
} // namespace recoverysuite