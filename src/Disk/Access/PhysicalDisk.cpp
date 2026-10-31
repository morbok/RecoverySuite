#include "PhysicalDisk.h"
#include "../../Platform/IDiskEnumerator.h"
#include "../DiskEnumerator.h"
#include "read_only_device.h"
namespace recoverysuite::disk {
class PhysicalDisk::PhysicalDiskImpl {
public:
  uint64_t number;
  ReadOnlyDevice reader;
  PhysicalDiskImpl(uint64_t n, bool readOnly) : number(n) {
    if (!readOnly)
      throw AccessDeniedException("Physical writes are disabled");
#ifdef _WIN32
    reader.open("\\\\.\\PhysicalDrive" + std::to_string(n));
#else
    reader.open(createDiskEnumerator()->getDiskInfo(n).devicePath);
#endif
  }
};
PhysicalDisk::PhysicalDisk(uint64_t n, bool readOnly)
    : pImpl(std::make_unique<PhysicalDiskImpl>(n, readOnly)) {}
PhysicalDisk::~PhysicalDisk() = default;
void PhysicalDisk::open(const std::string &path) { pImpl->reader.open(path); }
void PhysicalDisk::close() { pImpl->reader.close(); }
bool PhysicalDisk::isOpen() const noexcept { return pImpl->reader.isOpen(); }
uint64_t PhysicalDisk::getDiskNumber() const { return pImpl->number; }
bool PhysicalDisk::isReadOnly() const { return true; }
DeviceInformation PhysicalDisk::getDiskInfo() const {
  auto info = pImpl->reader.getDiskInfo();
  info.diskNumber = pImpl->number;
  return info;
}
DeviceInformation PhysicalDisk::getDeviceInformation() const {
  return getDiskInfo();
}
uint32_t PhysicalDisk::getSectorSize() const noexcept {
  return pImpl->reader.getSectorSize();
}
uint32_t PhysicalDisk::getBytesPerSector() const { return getSectorSize(); }
uint64_t PhysicalDisk::getTotalSectors() const noexcept {
  return pImpl->reader.getTotalSectors();
}
uint64_t PhysicalDisk::getTotalSizeBytes() const {
  return getTotalSectors() * getSectorSize();
}
bool PhysicalDisk::readSectors(uint64_t start, uint64_t count,
                               std::vector<std::byte> &bytes) {
  return pImpl->reader.readSectors(start, count, bytes);
}
bool PhysicalDisk::writeSectors(uint64_t start, uint64_t count,
                                const std::vector<std::byte> &bytes) {
  return pImpl->reader.writeSectors(start, count, bytes);
}
DriveGeometry PhysicalDisk::getDriveGeometry() const {
  throw DiskException("CHS geometry is not measured; use the observed byte "
                      "capacity and sector size");
}
} // namespace recoverysuite::disk
