#include "LinuxDiskEnumerator.h"
#include "../../../Disk/Access/path_utf8.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
namespace recoverysuite::platform::linux {
namespace {
std::string text(const std::filesystem::path &path) {
  std::ifstream file(path);
  std::string value;
  std::getline(file, value);
  while (!value.empty() &&
         (value.back() == ' ' || value.back() == '\r' || value.back() == '\n'))
    value.pop_back();
  return value;
}
uint64_t number(const std::string &value) {
  uint64_t n{};
  const auto [end, error] =
      std::from_chars(value.data(), value.data() + value.size(), n);
  if (error != std::errc{} || end != value.data() + value.size() ||
      value.empty())
    throw std::runtime_error("Invalid sysfs geometry");
  return n;
}
} // namespace
std::vector<disk::DeviceInformation> LinuxDiskEnumerator::enumerateDisks() {
  std::vector<disk::DeviceInformation> result;
  std::error_code error;
  if (!std::filesystem::is_directory(sysfs_, error))
    throw disk::DiskException("Linux sysfs block inventory is unavailable");
  for (const auto &entry : std::filesystem::directory_iterator(sysfs_)) {
    if (std::filesystem::exists(entry.path() / "partition"))
      continue;
    try {
      const auto name = entry.path().filename().string();
      if (name.empty() ||
          name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRS"
                                 "TUVWXYZ0123456789_-.:") != std::string::npos)
        continue;
      const auto identity = text(entry.path() / "dev");
      const auto colon = identity.find(':');
      if (colon == std::string::npos)
        continue;
      const auto major = number(identity.substr(0, colon)),
                 minor = number(identity.substr(colon + 1));
      if (major > UINT32_MAX || minor > UINT32_MAX)
        continue;
      const auto blocks = number(text(entry.path() / "size")),
                 sector =
                     number(text(entry.path() / "queue/logical_block_size"));
      if (blocks > std::numeric_limits<uint64_t>::max() / 512 || sector < 512 ||
          sector > 4096 || (sector & (sector - 1)) || blocks * 512 % sector)
        continue;
      disk::DeviceInformation info;
      info.devicePath = disk::utf8Path(devices_ / name);
      info.diskNumber = (major << 32) | minor;
      info.totalSizeBytes = blocks * 512;
      info.bytesPerSector = sector;
      info.logicalBytesPerSector = uint32_t(sector);
      info.totalSectors = info.totalSizeBytes / sector;
      info.modelNumber = text(entry.path() / "device/model");
      if (info.modelNumber.empty())
        info.modelNumber = name;
      info.serialNumber = text(entry.path() / "device/serial");
      info.firmwareRevision = text(entry.path() / "device/rev");
      info.isReadOnly = text(entry.path() / "ro") == "1";
      info.isRemovable = text(entry.path() / "removable") == "1";
      const auto physical = text(entry.path() / "queue/physical_block_size");
      if (!physical.empty()) {
        const auto size = number(physical);
        if (size <= UINT32_MAX)
          info.physicalBytesPerSector = uint32_t(size);
      }
      result.push_back(std::move(info));
    } catch (const std::exception &) { /* An incomplete sysfs entry can
                                          disappear during enumeration. */
    }
  }
  std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
    return a.diskNumber < b.diskNumber;
  });
  return result;
}
disk::DeviceInformation LinuxDiskEnumerator::getDiskInfo(uint64_t diskNumber) {
  for (const auto &info : enumerateDisks())
    if (info.diskNumber == diskNumber)
      return info;
  throw disk::DiskNotFoundException(
      "Disk is absent from the current sysfs inventory");
}
} // namespace recoverysuite::platform::linux
