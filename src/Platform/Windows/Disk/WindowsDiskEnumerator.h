#pragma once
#include "../../IDiskEnumerator.h"
namespace recoverysuite::platform::windows {
class WindowsDiskEnumerator : public recoverysuite::platform::IDiskEnumerator {
public:
  WindowsDiskEnumerator() = default;
  ~WindowsDiskEnumerator() override = default;
  std::vector<disk::DeviceInformation> enumerateDisks() override;
  disk::DeviceInformation getDiskInfo(uint64_t diskNumber) override;
  uint64_t getDiskCount() override { return enumerateDisks().size(); }
};
} // namespace recoverysuite::platform::windows
