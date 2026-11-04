#include "Filesystem/FAT/FATFilesystem.h"
#include "Filesystem/FAT/FATMetadata.h"
#include "Filesystem/FAT/FATVolume.h"
#include "Recovery/fat_root_files.h"
#include "Recovery/ntfs_mft.h"
#include "Recovery/signature_carver.h"
#include "RecoverySuite/Partition/GPTParser.hpp"
#include "RecoverySuite/Partition/PartitionManager.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

using namespace recoverysuite;
namespace {
class MemorySource final : public disk::IDiskReader {
  std::vector<std::byte> bytes_;
  uint32_t sector_;

public:
  MemorySource(const uint8_t *input, size_t size)
      : sector_(size > 12 && input[11] == 0 && input[12] == 16 ? 4096 : 512) {
    bytes_.resize(((size + sector_ - 1) / sector_) * sector_);
    std::memcpy(bytes_.data(), input, size);
  }
  void open(const std::string &) override {}
  void close() override {}
  bool isOpen() const noexcept override { return true; }
  uint32_t getSectorSize() const noexcept override { return sector_; }
  uint64_t getTotalSectors() const noexcept override {
    return bytes_.size() / sector_;
  }
  disk::DeviceInformation getDiskInfo() const override { return {}; }
  bool writeSectors(uint64_t, uint64_t,
                    const std::vector<std::byte> &) override {
    throw std::logic_error("read-only");
  }
  bool readSectors(uint64_t start, uint64_t count,
                   std::vector<std::byte> &output) override {
    if (start > getTotalSectors() || count > getTotalSectors() - start)
      return false;
    output.assign(bytes_.begin() + static_cast<ptrdiff_t>(start * sector_),
                  bytes_.begin() +
                      static_cast<ptrdiff_t>((start + count) * sector_));
    return true;
  }
};
class LegacySource final : public filesystem::FilesystemReader {
  MemorySource &source_;

public:
  explicit LegacySource(MemorySource &source) : source_(source) {}
  uint32_t readSector(uint64_t sector, uint8_t *output,
                      uint32_t size) override {
    return readSectors(sector, 1, output, size);
  }
  uint32_t readSectors(uint64_t start, uint32_t count, uint8_t *output,
                       uint32_t size) override {
    if (size != getSectorSize())
      return 0;
    std::vector<std::byte> bytes;
    if (!source_.readSectors(start, count, bytes))
      return 0;
    std::memcpy(output, bytes.data(), bytes.size());
    return static_cast<uint32_t>(bytes.size());
  }
  uint32_t readCluster(uint64_t, uint8_t *, uint32_t) override { return 0; }
  uint32_t readClusters(uint64_t, uint32_t, uint8_t *, uint32_t) override {
    return 0;
  }
  uint64_t getTotalSectors() const override {
    return source_.getTotalSectors();
  }
  uint32_t getSectorSize() const override { return source_.getSectorSize(); }
  uint64_t getStartOffset() const override { return 0; }
};
template <class F> void parse(F &&parser) {
  // Malformed/unsupported images are expected to fail validation. Sanitizer
  // findings still abort.
  try {
    parser();
  } catch (const std::runtime_error &) {
  } catch (const std::invalid_argument &) {
  }
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *input, size_t size) {
  if (!size || size > 262144)
    return 0;
  MemorySource source(input, size);
  auto legacy = std::make_shared<LegacySource>(source);
  parse([&] { filesystem::fat::FATMetadata metadata(*legacy); });
  parse([&] {
    filesystem::fat::FATFilesystem fat(legacy);
    fat.getType();
    fat.getMetadata();
    auto volume = fat.getVolume();
    volume->isValid();
    volume->getEndOffset();
  });
  const auto count = source.getTotalSectors();
  const auto borrowed =
      std::shared_ptr<disk::IDiskReader>(&source, [](disk::IDiskReader *) {});
  parse([&] { partition::PartitionManager(borrowed).validatePartitions(); });
  parse([&] { partition::GPTParser(borrowed).parseGPT(); });
  const auto discard = [](const std::string &, const std::vector<uint8_t> &) {};
  parse([&] { recovery::carveSignatures(source, 0, count, discard); });
  parse([&] {
    recovery::recoverFatRootFiles(source, 0, count, discard, {}, {}, true,
                                  false);
  });
  parse([&] {
    recovery::recoverFatRootFiles(source, 0, count, discard, {}, {}, true,
                                  true);
  });
  parse([&] { recovery::readNtfsMft(source, 0, count, {}, {}, 262144); });
  parse([&] { recovery::recoverNtfsRootFiles(source, 0, count, discard); });
  parse([&] { recovery::recoverNtfsTreeFiles(source, 0, count, discard); });
  for (const bool deleted : {false, true})
    parse([&] {
      recovery::recoverNtfsChecked(
          source, 0, count, {true, true, deleted},
          [](const recovery::NtfsFileSource &file) {
            if (file.size)
              file.read(
                  0, static_cast<size_t>(std::min<uint64_t>(file.size, 65536)));
          });
    });
  return 0;
}
