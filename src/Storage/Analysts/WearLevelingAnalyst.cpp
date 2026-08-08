#include "WearLevelingAnalyst.h"
#include <stdexcept>
#include <memory>

#include "StorageAccess.h"
#include "StorageUtils.h"

namespace recoverysuite {
namespace storage {

class WearLevelingAnalyst::WearLevelingAnalystImpl {
public:
    WearLevelingAnalystImpl()
        : storageAccess(std::make_unique<StorageAccess>()),
          storageUtils(std::make_unique<StorageUtils>()) {}

    std::unique_ptr<StorageAccess> storageAccess;
    std::unique_ptr<StorageUtils> storageUtils;
};

WearLevelingAnalyst::WearLevelingAnalyst() : pImpl(std::make_unique<WearLevelingAnalystImpl>()) {}


WearLevelingQuality WearLevelingAnalyst::analyze(uint64_t diskNumber) {
    try {
        // Delegate to storage access layer to get wear leveling quality
        return pImpl->storageAccess->getWearLevelingQuality(diskNumber);
    } catch (const StorageException& ex) {
        throw StorageException("Wear leveling analysis failed: " + std::string(ex.what()));
    } catch (const std::exception& ex) {
        throw StorageException("Wear leveling analysis failed: " + std::string(ex.what()));
    }
}

} // namespace storage
} // namespace recoverysuite