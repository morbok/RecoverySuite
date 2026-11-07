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
        storageInfo.device_path = diskInfo.devicePath;
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
        storageInfo.nand_capacity_bytes = 0; // Unknown: physical NAND capacity is not measured.

        return storageInfo;
    } catch (const recoverysuite::disk::DiskException& ex) {
        throw StorageException("Failed to get device info: " + std::string(ex.what()), ex.getErrorCode());
    } catch (const std::exception& ex) {
        throw StorageException("Failed to get device info: " + std::string(ex.what()));
    }
}

StorageHealthData StorageAccess::getHealthData(uint64_t) {throw StorageException("SMART/NVMe health has not been measured by this build");}
uint32_t StorageAccess::sendVendorCommand(uint64_t,const std::vector<uint8_t>&,std::vector<uint8_t>&) {throw StorageException("Vendor commands are unavailable in the read-only workflow");}

TRIMStatus StorageAccess::getTrimStatus(uint64_t /*diskNumber*/) {
    // Default implementation: unknown
    // Platform-specific implementations should override this method
    return TRIMStatus::UNKNOWN;
}

WearLevelingQuality StorageAccess::getWearLevelingQuality(uint64_t /*diskNumber*/) {
    // Default implementation: unknown
    // Platform-specific implementations should override this method
    return WearLevelingQuality::UNKNOWN;
}

GarbageCollectionAggressiveness StorageAccess::getGcAggressiveness(uint64_t /*diskNumber*/) {
    // Default implementation: unknown
    // Platform-specific implementations should override this method
    return GarbageCollectionAggressiveness::UNKNOWN;
}

} // namespace storage
} // namespace recoverysuite