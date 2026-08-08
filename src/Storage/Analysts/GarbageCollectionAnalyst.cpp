#include "GarbageCollectionAnalyst.h"
#include <stdexcept>
#include <memory>

#include "StorageAccess.h"
#include "StorageUtils.h"

namespace recoverysuite {
namespace storage {

class GarbageCollectionAnalyst::GarbageCollectionAnalystImpl {
public:
    GarbageCollectionAnalystImpl()
        : storageAccess(std::make_unique<StorageAccess>()),
          storageUtils(std::make_unique<StorageUtils>()) {}

    std::unique_ptr<StorageAccess> storageAccess;
    std::unique_ptr<StorageUtils> storageUtils;
};

GarbageCollectionAnalyst::GarbageCollectionAnalyst() : pImpl(std::make_unique<GarbageCollectionAnalystImpl>()) {}


GarbageCollectionAggressiveness GarbageCollectionAnalyst::analyze(uint64_t diskNumber) {
    try {
        // Delegate to storage access layer to get GC aggressiveness
        return pImpl->storageAccess->getGcAggressiveness(diskNumber);
    } catch (const StorageException& ex) {
        throw StorageException("Garbage collection analysis failed: " + std::string(ex.what()));
    } catch (const std::exception& ex) {
        throw StorageException("Garbage collection analysis failed: " + std::string(ex.what()));
    }
}

} // namespace storage
} // namespace recoverysuite