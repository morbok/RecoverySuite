#pragma once
#include "Disk/IDiskReader.hpp"
#include <memory>
namespace recoverysuite::disk {
// Only opens real block/PhysicalDrive devices read-only; never changes their
// state or sends writes. Capture requires an unmounted/offline source and a
// destination on a different physical device.
class ReadOnlyDevice final : public IDiskReader {
public:
  ReadOnlyDevice();
  ~ReadOnlyDevice() override;
  void open(const std::string &path) override;
  void close() override;
  bool isOpen() const noexcept override;
  uint32_t getSectorSize() const noexcept override;
  uint64_t getTotalSectors() const noexcept override;
  DeviceInformation getDiskInfo() const override;
  bool readSectors(uint64_t start, uint64_t count,
                   std::vector<std::byte> &bytes) override;
  bool writeSectors(uint64_t, uint64_t,
                    const std::vector<std::byte> &) override;
  void validateDestination(const std::string &directory) const;
  void validateIdentity() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace recoverysuite::disk
