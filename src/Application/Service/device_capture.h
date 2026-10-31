#pragma once
#include "Disk/IDiskReader.hpp"
#include <functional>
#include <string>
namespace recoverysuite::application::service {
struct CaptureResult {
  uint64_t bytes{};
  uint32_t sectorSize{};
  std::string sha256;
};
// Trusted read-only source -> exclusive regular image, verified in bounded 64
// KiB pulls.
CaptureResult
copySourceImage(disk::IDiskReader &source, const std::string &output,
                const std::function<void(uint64_t)> &progress = {},
                const std::function<bool()> &cancelled = {});
// Real devices only. Enforces unmounted/offline source and different physical
// destination.
CaptureResult captureDevice(const std::string &device,
                            const std::string &output,
                            const std::function<void(uint64_t)> &progress = {},
                            const std::function<bool()> &cancelled = {});
} // namespace recoverysuite::application::service
