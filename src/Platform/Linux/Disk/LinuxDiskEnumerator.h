#pragma once
#include "../../IDiskEnumerator.h"
#include <filesystem>
namespace recoverysuite::platform::linux {
// Reads observed sysfs geometry/identity only; opening a device is a separate
// permission-checked step.
class LinuxDiskEnumerator : public recoverysuite::platform::IDiskEnumerator {
public:
  explicit LinuxDiskEnumerator(std::filesystem::path sysfs = "/sys/class/block",
                               std::filesystem::path devices = "/dev")
      : sysfs_(std::move(sysfs)), devices_(std::move(devices)) {}
  std::vector<disk::DeviceInformation> enumerateDisks() override;
  disk::DeviceInformation getDiskInfo(uint64_t diskNumber) override;
  uint64_t getDiskCount() override { return enumerateDisks().size(); }

private:
  std::filesystem::path sysfs_, devices_;
};
} // namespace recoverysuite::platform::linux
