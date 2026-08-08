#include "StorageUtils.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <cctype>

namespace recoverysuite {
namespace storage {

StorageUtils::StorageUtils() {}

StorageUtils::~StorageUtils() {}

std::string StorageUtils::formatBytes(uint64_t bytes) {
    const char* suffix[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int i = 0;
    double value = static_cast<double>(bytes);

    while (value >= 1024.0 && i < 5) {
        value /= 1024.0;
        i++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << value << " " << suffix[i];
    return oss.str();
}

double StorageUtils::kelvinToCelsius(uint16_t kelvin) {
    return static_cast<double>(kelvin) - 273.15;
}

uint16_t StorageUtils::celsiusToKelvin(double celsius) {
    return static_cast<uint16_t>(std::round(celsius + 273.15));
}

void StorageUtils::parseVendorModel(const std::string& identification,
                                   std::string& vendor,
                                   std::string& model) {
    // Simple implementation - in reality this would be much more sophisticated
    // and would use a database of known vendor/model strings

    vendor = "";
    model = identification; // Default to full string if we can't parse

    // Trim whitespace
    auto start = identification.find_first_not_of(" \t\n\r");
    auto end = identification.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return; // Empty or whitespace only
    }

    std::string trimmed = identification.substr(start, end - start + 1);

    // Look for common patterns like "Vendor Model Number" or "Vendor-Model"
    size_t spacePos = trimmed.find(' ');
    if (spacePos != std::string::npos) {
        vendor = trimmed.substr(0, spacePos);
        model = trimmed.substr(spacePos + 1);
    } else {
        size_t dashPos = trimmed.find('-');
        if (dashPos != std::string::npos) {
            vendor = trimmed.substr(0, dashPos);
            model = trimmed.substr(dashPos + 1);
        }
    }

    // If still empty, make reasonable defaults
    if (vendor.empty()) {
        vendor = "Unknown";
    }
    if (model.empty()) {
        model = trimmed;
    }
}

double StorageUtils::calculateWriteAmplification(double hostWrites, double nandWrites) {
    if (nandWrites <= 0.0) {
        return 0.0; // Avoid division by zero
    }
    return hostWrites / nandWrites;
}

double StorageUtils::estimateOverprovisioningPct(uint64_t userCapacity, uint64_t rawNandCapacity) {
    if (rawNandCapacity <= userCapacity) {
        return 0.0; // No over-provisioning or invalid values
    }

    return static_cast<double>(rawNandCapacity - userCapacity) /
           static_cast<double>(rawNandCapacity) * 100.0;
}

bool StorageUtils::isLikelySsd(const StorageDeviceInfo& deviceInfo) {
    // Heuristic to determine if device is likely an SSD

    // Check interface type - certain interfaces strongly suggest SSD
    switch (deviceInfo.interface_type) {
        case StorageInterfaceType::NVMe:
        case StorageInterfaceType::USB_NVMe:
        case StorageInterfaceType::PCIe:
            return true; // NVMe and PCIe are almost always SSD
        case StorageInterfaceType::SATA:
        case StorageInterfaceType::USB_SATA:
            // SATA could be SSD or HDD, need additional checks
            break;
        default:
            return false; // Other interfaces unlikely to be SSD
    }

    // Check rotational rate if available (not in our current struct)
    // In a real implementation, we'd check for non-rotational media

    // Check access time characteristics - SSDs have much lower access times
    // Not available in our current struct

    // For now, make a simple heuristic based on model number
    std::string lowerModel = deviceInfo.model;
    std::transform(lowerModel.begin(), lowerModel.end(), lowerModel.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    // Common SSD indicators in model names
    const std::vector<std::string> ssdIndicators = {
        "ssd", "nvme", "m.2", "flash", "solid state"
    };

    for (const auto& indicator : ssdIndicators) {
        if (lowerModel.find(indicator) != std::string::npos) {
            return true;
        }
    }

    // Common HDD indicators in model names
    const std::vector<std::string> hddIndicators = {
        "hdd", "hard drive", "desktop", "laptop", " Surveillance", " NAS"
    };

    for (const auto& indicator : hddIndicators) {
        if (lowerModel.find(indicator) != std::string::npos) {
            return false;
        }
    }

    // Default to assuming SATA devices are HDDs if we can't determine
    // (This is conservative - better to assume HDD when uncertain for safety)
    if (deviceInfo.interface_type == StorageInterfaceType::SATA ||
        deviceInfo.interface_type == StorageInterfaceType::USB_SATA) {
        return false;
    }

    return false; // Default to false if we can't determine
}

} // namespace storage
} // namespace recoverysuite