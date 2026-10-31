#include "Recovery/fat_root_files.h"
#include "Recovery/ntfs_mft.h"
#include "Recovery/signature_carver.h"
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
template <class F> void parse(F &&parser) {
  // Malformed/unsupported images are expected to fail validation. Sanitizer
  // findings still abort.
  try {
    parser();
  } catch (const std::exception &) {
  }
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *input, size_t size) {
  if (!size || size > 262144)
    return 0;
  MemorySource source(input, size);
  const auto count = source.getTotalSectors();
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
