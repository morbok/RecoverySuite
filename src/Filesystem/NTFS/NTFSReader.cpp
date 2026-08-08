#include "NTFSReader.h"
#include <stdexcept>

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

NTFSReader::NTFSReader(std::shared_ptr<FilesystemReader> underlyingReader)
    : underlyingReader_(std::move(underlyingReader)), 
      context_(std::make_shared<NTFSContext>(underlyingReader_)) {}


uint32_t NTFSReader::readSector(uint64_t sectorIndex, uint8_t* buffer, uint32_t sectorSize) {
    if (!underlyingReader_) {
        return 0;
    }
    return underlyingReader_->readSector(sectorIndex, buffer, sectorSize);
}

uint32_t NTFSReader::readSectors(uint64_t startSector, uint32_t sectorCount, uint8_t* buffer, uint32_t sectorSize) {
    if (!underlyingReader_) {
        return 0;
    }
    return underlyingReader_->readSectors(startSector, sectorCount, buffer, sectorSize);
}

uint32_t NTFSReader::readCluster(uint64_t clusterIndex, uint8_t* buffer, uint32_t clusterSize) {
    if (!underlyingReader_) {
        return 0;
    }
    return underlyingReader_->readCluster(clusterIndex, buffer, clusterSize);
}

uint32_t NTFSReader::readClusters(uint64_t startCluster, uint32_t clusterCount, uint8_t* buffer, uint32_t clusterSize) {
    if (!underlyingReader_) {
        return 0;
    }
    return underlyingReader_->readClusters(startCluster, clusterCount, buffer, clusterSize);
}

uint64_t NTFSReader::getTotalSectors() const {
    if (!underlyingReader_) {
        return 0;
    }
    return underlyingReader_->getTotalSectors();
}

uint32_t NTFSReader::getSectorSize() const {
    if (!underlyingReader_) {
        return 512; // Default sector size
    }
    return underlyingReader_->getSectorSize();
}

void NTFSReader::flush() {
    if (underlyingReader_) {
        underlyingReader_->flush();
    }
}

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite
