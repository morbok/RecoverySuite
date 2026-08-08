#include "StorageManager.h"
#include <stdexcept>
#include <memory>

#include "Access/StorageAccess.h"
#include "Reporting/StorageReporter.h"
#include "Utils/StorageUtils.h"

namespace recoverysuite {
namespace storage {

class StorageManager::StorageManagerImpl {
public:
    StorageManagerImpl()
        : storageAccess(std::make_unique<StorageAccess>()),
          reporter(std::make_unique<StorageReporter>()),
          utils(std::make_unique<StorageUtils>()) {}

    std::unique_ptr<StorageAccess> storageAccess;
    std::unique_ptr<StorageReporter> reporter;
    std::unique_ptr<StorageUtils> utils;
};

StorageManager::StorageManager() : pImpl(std::make_unique<StorageManagerImpl>()) {}

StorageManager::~StorageManager() = default;

StorageAnalysisReport StorageManager::analyzeStorage(uint64_t diskNumber) {
    // Get basic device information
    StorageDeviceInfo deviceInfo = getStorageInfo(diskNumber);

    // Perform various analyses using storage access layer
    TRIMStatus trimStatus = pImpl->storageAccess->getTrimStatus(diskNumber);
    WearLevelingQuality wearLeveling = pImpl->storageAccess->getWearLevelingQuality(diskNumber);
    GarbageCollectionAggressiveness gcAggressiveness = pImpl->storageAccess->getGcAggressiveness(diskNumber);

    // Get health data
    StorageHealthData healthData = pImpl->storageAccess->getHealthData(diskNumber);

    // Generate report
    return pImpl->reporter->generateReport(
        deviceInfo,
        trimStatus,
        wearLeveling,
        gcAggressiveness,
        healthData
    );
}

StorageDeviceInfo StorageManager::getStorageInfo(uint64_t diskNumber) {
    // Delegate to storage access layer
    return pImpl->storageAccess->getDeviceInfo(diskNumber);
}

TRIMStatus StorageManager::checkTrimStatus(uint64_t diskNumber) {
    // Delegate to storage access layer
    return pImpl->storageAccess->getTrimStatus(diskNumber);
}

WearLevelingQuality StorageManager::assessWearLeveling(uint64_t diskNumber) {
    // Delegate to storage access layer
    return pImpl->storageAccess->getWearLevelingQuality(diskNumber);
}

GarbageCollectionAggressiveness StorageManager::assessGcAggressiveness(uint64_t diskNumber) {
    // Delegate to storage access layer
    return pImpl->storageAccess->getGcAggressiveness(diskNumber);
}

} // namespace storage
} // namespace recoverysuite