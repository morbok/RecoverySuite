#pragma once
#include "Disk/IDiskReader.hpp"
#include <cstdio>
#include <mutex>
namespace recoverysuite::disk {
/** Read-only regular disk image. Reads are serialized; open/close require exclusive ownership.
 * Sector size must be supplied explicitly; no physical devices or partial final sectors are accepted.
 */
class DiskImageReader final : public IDiskReader {
public:
    explicit DiskImageReader(uint32_t sectorSize = 512);
    ~DiskImageReader() override;
    void open(const std::string& path) override;
    void close() override;
    bool isOpen() const noexcept override;
    bool readSectors(uint64_t start, uint64_t count, std::vector<std::byte>& buffer) override;
    bool writeSectors(uint64_t, uint64_t, const std::vector<std::byte>&) override;
    DeviceInformation getDiskInfo() const override;
    uint32_t getSectorSize() const noexcept override;
    uint64_t getTotalSectors() const noexcept override;
    const std::string& sourcePath() const noexcept;
    /** Reject a replaced, resized or modified source before reading from its retained handle. */
    void validateIdentity() const;
private:
    struct FileCloser { void operator()(std::FILE* file) const noexcept { if (file) std::fclose(file); } };
    std::unique_ptr<std::FILE, FileCloser> file_;
    uint64_t device_{}, inode_{}, modifiedSeconds_{}, modifiedNanos_{};
    uint32_t sectorSize_;
    uint64_t size_{};
    std::string path_;
    mutable std::mutex mutex_;
};
}
