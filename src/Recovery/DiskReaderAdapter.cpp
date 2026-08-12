#include "DiskReaderAdapter.hpp"
#include <stdexcept>
#include <cstring>

namespace recoverysuite {
namespace recovery {

DiskReaderAdapter::DiskReaderAdapter(recoverysuite::disk::IDiskReader* diskReader, uint64_t startOffset)
    : diskReader_(diskReader), startOffset_(startOffset), sectorSize_(0) {
    if (!diskReader_) {
        throw std::invalid_argument("DiskReaderAdapter: diskReader is null");
    }
    // Cache sector size
    sectorSize_ = diskReader_->getSectorSize();
    if (sectorSize_ == 0) {
        throw std::runtime_error("DiskReaderAdapter: Invalid sector size from disk reader");
    }
}

uint32_t DiskReaderAdapter::readSector(uint64_t sectorIndex, uint8_t* buffer, uint32_t sectorSize) {
    if (!diskReader_) {
        return 0;
    }
    if (sectorSize != sectorSize_) {
        // Sector size mismatch
        return 0;
    }
    // Convert filesystem sector index to disk sector index
    uint64_t diskSectorIndex = startOffset_ / sectorSize_ + sectorIndex;
    std::vector<std::byte> byteBuffer(sectorSize_);
    bool success = diskReader_->readSectors(diskSectorIndex, 1, byteBuffer);
    if (!success) {
        return 0;
    }
    std::memcpy(buffer, byteBuffer.data(), sectorSize);
    return sectorSize;
}

uint32_t DiskReaderAdapter::readSectors(uint64_t startSector, uint32_t sectorCount, uint8_t* buffer, uint32_t sectorSize) {
    if (!diskReader_) {
        return 0;
    }
    if (sectorSize != sectorSize_) {
        return 0;
    }
    if (sectorCount == 0) {
        return 0;
    }
    uint64_t diskSectorIndex = startOffset_ / sectorSize_ + startSector;
    std::vector<std::byte> byteBuffer(sectorCount * sectorSize_);
    bool success = diskReader_->readSectors(diskSectorIndex, sectorCount, byteBuffer);
    if (!success) {
        return 0;
    }
    std::memcpy(buffer, byteBuffer.data(), sectorCount * sectorSize);
    return sectorCount * sectorSize;
}

uint32_t DiskReaderAdapter::readCluster(uint64_t clusterIndex, uint8_t* buffer, uint32_t clusterSize) {
    // This implementation assumes that the filesystem layer will not call readCluster/readClusters
    // because we are using the adapter primarily for metadata recovery which uses sector reads.
    // For simplicity, we return 0 to indicate not implemented.
    (void)clusterIndex;
    (void)buffer;
    (void)clusterSize;
    return 0;
}

uint32_t DiskReaderAdapter::readClusters(uint64_t startCluster, uint32_t clusterCount, uint8_t* buffer, uint32_t clusterSize) {
    (void)startCluster;
    (void)clusterCount;
    (void)buffer;
    (void)clusterSize;
    return 0;
}

uint64_t DiskReaderAdapter::getTotalSectors() const {
    if (!diskReader_) {
        return 0;
    }
    uint64_t diskTotalSectors = diskReader_->getTotalSectors();
    if (diskTotalSectors < startOffset_ / sectorSize_) {
        return 0;
    }
    return diskTotalSectors - (startOffset_ / sectorSize_);
}

uint32_t DiskReaderAdapter::getSectorSize() const {
    return sectorSize_;
}

uint64_t DiskReaderAdapter::getStartOffset() const {
    return startOffset_;
}

} // namespace recovery
} // namespace recoverysuite