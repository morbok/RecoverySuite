#include "StorageAccess.h"
#include <stdexcept>
#include <memory>
#include <vector>

#include "../../Disk/Access/PhysicalDisk.h"
#include "../../Disk/DiskExceptions.h"

namespace recoverysuite {
namespace storage {

class StorageAccess::StorageAccessImpl {
public:
    StorageAccessImpl()
        : diskManager(std::make_unique<recoverysuite::disk::DiskManager>()) {}

    std::unique_ptr<recoverysuite::disk::DiskManager> diskManager;
};

StorageAccess::StorageAccess() : pImpl(std::make_unique<StorageAccessImpl>()) {}

StorageAccess::~StorageAccess() = default;

StorageDeviceInfo StorageAccess::getDeviceInfo(uint64_t diskNumber) {
    try {
        // Get basic disk information from the disk layer
        auto diskInfo = pImpl->diskManager->getDiskInfo(diskNumber);

        // Convert Disk layer DeviceInformation to Storage layer StorageDeviceInfo
        StorageDeviceInfo storageInfo;
        storageInfo.device_path = "\\\\.\\PhysicalDrive" + std::to_string(diskNumber); // Windows-specific
        storageInfo.user_capacity_bytes = diskInfo.totalSizeBytes;
        storageInfo.sector_size_bytes = diskInfo.bytesPerSector;
        storageInfo.serial_number = diskInfo.serialNumber;
        storageInfo.model = diskInfo.modelNumber;
        storageInfo.vendor = ""; // Manufacturer not directly available in DeviceInformation
        storageInfo.revision = diskInfo.firmwareRevision;

        // Determine interface type based on bus type
        switch (diskInfo.busType) {
            case recoverysuite::disk::BusType::SATA:
                storageInfo.interface_type = StorageInterfaceType::SATA;
                break;
            case recoverysuite::disk::BusType::USB:
                // For USB, we'd need to determine if it's SATA or NVMe behind USB
                // For now, default to unknown - would be enhanced in real implementation
                storageInfo.interface_type = StorageInterfaceType::UNKNOWN;
                break;
            case recoverysuite::disk::BusType::Unknown:
            default:
                storageInfo.interface_type = StorageInterfaceType::UNKNOWN;
                break;
        }

        // Set default values for fields not available in DiskInformation
        storageInfo.form_factor = "Unknown";
        storageInfo.is_removable = diskInfo.isRemovable;
        storageInfo.block_size_bytes = diskInfo.bytesPerSector;
        storageInfo.nand_capacity_bytes = storageInfo.user_capacity_bytes; // Simplified

        return storageInfo;
    } catch (const recoverysuite::disk::DiskException& ex) {
        throw StorageException("Failed to get device info: " + std::string(ex.what()), ex.getErrorCode());
    } catch (const std::exception& ex) {
        throw StorageException("Failed to get device info: " + std::string(ex.what()));
    }
}

StorageHealthData StorageAccess::getHealthData(uint64_t diskNumber) {
    StorageHealthData healthData{};
    // Initialize with zeros/defaults
    healthData.critical_warning = 0;
    healthData.temperature_kelvin = 0;
    healthData.available_spare_percent = 0;
    healthData.spare_threshold_percent = 0;
    healthData.percentage_used = 0;
    healthData.data_read_gb = 0;
    healthData.data_written_gb = 0;
    healthData.host_read_commands = 0;
    healthData.host_write_commands = 0;
    healthData.power_cycles = 0;
    healthData.power_on_hours = 0;
    healthData.unsafe_shutdowns = 0;
    healthData.media_errors = 0;
    healthData.error_log_entries = 0;

    try {
        // Try to get SMART data from the disk
        auto physicalDisk = pImpl->diskManager->openDisk(diskNumber, true); // Read-only

        // In a real implementation, we would issue SMART commands here
        // For now, we'll return zeros indicating no data available
        // This would be enhanced with actual SMART command implementation

    } catch (const recoverysuite::disk::AccessDeniedException& ex) {
        throw AccessDeniedException("Access denied to disk " + std::to_string(diskNumber) +
                                  ": " + std::string(ex.what()), ex.getErrorCode());
    } catch (const recoverysuite::disk::DiskNotFoundException& ex) {
        throw DeviceNotFoundException("Disk " + std::to_string(diskNumber) + " not found: " +
                                    std::string(ex.what()), ex.getErrorCode());
    } catch (const recoverysuite::disk::DiskException& ex) {
        throw StorageException("Failed to get health data: " + std::string(ex.what()), ex.getErrorCode());
    } catch (const std::exception& ex) {
        throw StorageException("Failed to get health data: " + std::string(ex.what()));
    }

    return healthData;
}

uint32_t StorageAccess::sendVendorCommand(
    uint64_t diskNumber,
    const std::vector<uint8_t>& command,
    std::vector<uint8_t>& buffer) {
    try {
        auto physicalDisk = pImpl->diskManager->openDisk(diskNumber, true); // Read-only

        // In a real implementation, we would send the command through the physical disk interface
        // For now, return 0 indicating no data transferred
        // This would be enhanced with actual command implementation

        return 0;
    } catch (const recoverysuite::disk::AccessDeniedException& ex) {
        throw AccessDeniedException("Access denied to disk " + std::to_string(diskNumber) +
                                  ": " + std::string(ex.what()), ex.getErrorCode());
    } catch (const recoverysuite::disk::DiskNotFoundException& ex) {
        throw DeviceNotFoundException("Disk " + std::to_string(diskNumber) + " not found: " +
                                    std::string(ex.what()), ex.getErrorCode());
    } catch (const recoverysuite::disk::DiskException& ex) {
        throw StorageException("Failed to send vendor command: " + std::string(ex.what()), ex.getErrorCode());
    } catch (const std::exception& ex) {
        throw StorageException("Failed to send vendor command: " + std::string(ex.what()));
    }
}

TRIMStatus StorageAccess::getTrimStatus(uint64_t diskNumber) {
    // Default implementation: unknown
    // Platform-specific implementations should override this method
    return TRIMStatus::UNKNOWN;
}

WearLevelingQuality StorageAccess::getWearLevelingQuality(uint64_t diskNumber) {
    // Default implementation: unknown
    // Platform-specific implementations should override this method
    return WearLevelingQuality::UNKNOWN;
}

GarbageCollectionAggressiveness StorageAccess::getGcAggressiveness(uint64_t diskNumber) {
    // Default implementation: unknown
    // Platform-specific implementations should override this method
    return GarbageCollectionAggressiveness::UNKNOWN;
}

} // namespace storage
} // namespace recoverysuite