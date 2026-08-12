#ifndef RECOVERYSUITE_RECOVERY_DISKREADERADAPTER_HPP
#define RECOVERYSUITE_RECOVERY_DISKREADERADAPTER_HPP

#include <cstdint>
#include <memory>
#include <vector>
#include "../Disk/IDiskReader.hpp"
#include "../Filesystem/FilesystemReader.h"

namespace recoverysuite {
namespace recovery {

/**
 * @brief Adapter that converts IDiskReader to FilesystemReader with a start offset.
 *
 * This allows recovery components to use the filesystem layer classes (e.g., FATMetadata, NTFSMetadata)
 * by providing a FilesystemReader that reads from a specific offset within the disk.
 */
class DiskReaderAdapter : public recoverysuite::filesystem::FilesystemReader {
public:
    /**
     * @brief Construct a new DiskReaderAdapter
     *
     * @param diskReader The underlying disk reader (caller retains ownership)
     * @param startOffset The start offset in bytes (typically volume start offset)
     */
    DiskReaderAdapter(recoverysuite::disk::IDiskReader* diskReader, uint64_t startOffset);

    // Destructor
    ~DiskReaderAdapter() override = default;

    // FilesystemReader interface
    uint32_t readSector(uint64_t sectorIndex, uint8_t* buffer, uint32_t sectorSize) override;
    uint32_t readSectors(uint64_t startSector, uint32_t sectorCount, uint8_t* buffer, uint32_t sectorSize) override;
    uint32_t readCluster(uint64_t clusterIndex, uint8_t* buffer, uint32_t clusterSize) override;
    uint32_t readClusters(uint64_t startCluster, uint32_t clusterCount, uint8_t* buffer, uint32_t clusterSize) override;
    uint64_t getTotalSectors() const override;
    uint32_t getSectorSize() const override;
    uint64_t getStartOffset() const override;
    void flush() override {};

private:
    recoverysuite::disk::IDiskReader* diskReader_; // Not owned
    uint64_t startOffset_; // Start offset in bytes
    uint32_t sectorSize_;  // Sector size in bytes (cached)
};

} // namespace recovery
} // namespace recoverysuite

#endif // RECOVERYSUITE_RECOVERY_DISKREADERADAPTER_HPP