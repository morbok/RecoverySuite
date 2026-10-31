#include "WindowsStorageAccess.h"
namespace recoverysuite::storage {
class WindowsStorageAccess::WindowsStorageAccessImpl {};
WindowsStorageAccess::WindowsStorageAccess():pImpl(std::make_unique<WindowsStorageAccessImpl>()){}
WindowsStorageAccess::~WindowsStorageAccess()=default;
StorageDeviceInfo WindowsStorageAccess::getDeviceInfo(uint64_t number){return StorageAccess::getDeviceInfo(number);}
StorageHealthData WindowsStorageAccess::getHealthData(uint64_t){throw StorageException("SMART/NVMe health has not been measured by this build");}
TRIMStatus WindowsStorageAccess::getTrimStatus(uint64_t){return TRIMStatus::UNKNOWN;}
WearLevelingQuality WindowsStorageAccess::getWearLevelingQuality(uint64_t){return WearLevelingQuality::UNKNOWN;}
GarbageCollectionAggressiveness WindowsStorageAccess::getGcAggressiveness(uint64_t){return GarbageCollectionAggressiveness::UNKNOWN;}
uint32_t WindowsStorageAccess::sendVendorCommand(uint64_t,const std::vector<uint8_t>&,std::vector<uint8_t>&){throw StorageException("Vendor commands are unavailable in the read-only workflow");}
}
