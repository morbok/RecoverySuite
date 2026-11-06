#pragma once
#include "Disk/IDiskReader.hpp"
#include <functional>
#include <string>
namespace recoverysuite::application::service {
struct UnreadableRange {
  uint64_t sector{}, count{};
};
struct RescueResult {
  uint64_t bytes{}, unreadableSectors{};
  uint32_t sectorSize{};
  std::string sha256, report;
  std::vector<UnreadableRange> unreadable;
};
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
// Explicit salvage: sector fallback with <=5 retries, zero-filled unreadable
// ranges and a mandatory report. Verifies stored output without rereading
// media.
RescueResult
rescueSourceImage(disk::IDiskReader &source, const std::string &output,
                  unsigned retries = 2,
                  const std::function<void(uint64_t)> &progress = {},
                  const std::function<bool()> &cancelled = {});
RescueResult rescueDevice(const std::string &source, const std::string &output,
                          unsigned retries = 2,
                          const std::function<void(uint64_t)> &progress = {},
                          const std::function<bool()> &cancelled = {});
// Real devices only. Enforces unmounted/offline source and different physical
// destination.
CaptureResult captureDevice(const std::string &device,
                            const std::string &output,
                            const std::function<void(uint64_t)> &progress = {},
                            const std::function<bool()> &cancelled = {});
} // namespace recoverysuite::application::service
