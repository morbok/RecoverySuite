#include "TrimAnalyst.h"
#include <stdexcept>
#include <memory>

#include "StorageAccess.h"
#include "StorageUtils.h"

namespace recoverysuite {
namespace storage {

class TrimAnalyst::TrimAnalystImpl {
public:
    TrimAnalystImpl()
        : storageAccess(std::make_unique<StorageAccess>()),
          storageUtils(std::make_unique<StorageUtils>()) {}

    std::unique_ptr<StorageAccess> storageAccess;
    std::unique_ptr<StorageUtils> storageUtils;
};

TrimAnalyst::TrimAnalyst() : pImpl(std::make_unique<TrimAnalystImpl>()) {}


TRIMStatus TrimAnalyst::analyze(uint64_t diskNumber) {
    try {
        // Delegate to storage access layer to get TRIM status
        return pImpl->storageAccess->getTrimStatus(diskNumber);
    } catch (const StorageException& ex) {
        throw StorageException("TRIM analysis failed: " + std::string(ex.what()));
    } catch (const std::exception& ex) {
        throw StorageException("TRIM analysis failed: " + std::string(ex.what()));
    }
}

} // namespace storage
} // namespace recoverysuite