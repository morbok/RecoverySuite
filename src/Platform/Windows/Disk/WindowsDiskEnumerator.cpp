#include "WindowsDiskEnumerator.h"
#include <algorithm>
#include <cstddef>
#include <initguid.h>
#include <ntddstor.h>
#include <set>
#include <setupapi.h>
#include <vector>
#include <windows.h>
#include <winioctl.h>
namespace recoverysuite::platform::windows {
namespace {
struct Handle {
  HANDLE value;
  ~Handle() {
    if (value != INVALID_HANDLE_VALUE)
      CloseHandle(value);
  }
};
std::string field(const std::vector<uint8_t> &bytes, DWORD offset) {
  if (!offset || offset >= bytes.size())
    return {};
  auto end = std::find(bytes.begin() + offset, bytes.end(), uint8_t{0});
  if (end == bytes.end())
    return {};
  std::string value(bytes.begin() + offset, end);
  while (!value.empty() && value.back() == ' ')
    value.pop_back();
  return value;
}
} // namespace
disk::DeviceInformation WindowsDiskEnumerator::getDiskInfo(uint64_t number) {
  if (number > UINT32_MAX)
    throw disk::DiskNotFoundException("Invalid Windows disk number");
  const auto path = L"\\\\.\\PhysicalDrive" + std::to_wstring(number);
  Handle h{CreateFileW(path.c_str(), 0,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       nullptr, OPEN_EXISTING, 0, nullptr)};
  if (h.value == INVALID_HANDLE_VALUE)
    throw disk::DiskException(
        "Cannot query physical disk metadata; access denied or device missing",
        GetLastError());
  DWORD got{};
  DISK_GEOMETRY geometry{};
  GET_LENGTH_INFORMATION length{};
  alignas(DISK_GEOMETRY_EX) uint8_t geometryBytes[4096]{};
  STORAGE_DEVICE_NUMBER identity{};
  if (!DeviceIoControl(h.value, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0,
                       &identity, sizeof(identity), &got, nullptr) ||
      identity.DeviceType != FILE_DEVICE_DISK ||
      identity.DeviceNumber != number ||
      !DeviceIoControl(h.value, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0,
                       geometryBytes, sizeof(geometryBytes), &got, nullptr) ||
      got < offsetof(DISK_GEOMETRY_EX, Data))
    throw disk::DiskException("Cannot query physical geometry", GetLastError());
  geometry = reinterpret_cast<DISK_GEOMETRY_EX *>(geometryBytes)->Geometry;
  length.Length = reinterpret_cast<DISK_GEOMETRY_EX *>(geometryBytes)->DiskSize;
  if (length.Length.QuadPart <= 0 || geometry.BytesPerSector < 512 ||
      geometry.BytesPerSector > 4096 ||
      (geometry.BytesPerSector & (geometry.BytesPerSector - 1)) ||
      length.Length.QuadPart % geometry.BytesPerSector)
    throw disk::DiskException("Cannot query valid physical disk geometry",
                              GetLastError());
  disk::DeviceInformation info;
  info.devicePath = "\\\\.\\PhysicalDrive" + std::to_string(number);
  info.diskNumber = number;
  info.bytesPerSector = geometry.BytesPerSector;
  info.logicalBytesPerSector = geometry.BytesPerSector;
  info.totalSizeBytes = uint64_t(length.Length.QuadPart);
  info.totalSectors = info.totalSizeBytes / info.bytesPerSector;
  info.isReadOnly = true;
  info.modelNumber = "Unknown";
  STORAGE_PROPERTY_QUERY query{};
  query.PropertyId = StorageDeviceProperty;
  query.QueryType = PropertyStandardQuery;
  STORAGE_DESCRIPTOR_HEADER header{};
  if (DeviceIoControl(h.value, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                      sizeof(query), &header, sizeof(header), &got, nullptr) &&
      header.Size >= sizeof(STORAGE_DEVICE_DESCRIPTOR) &&
      header.Size <= 65536) {
    std::vector<uint8_t> bytes(header.Size);
    if (DeviceIoControl(h.value, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                        sizeof(query), bytes.data(), DWORD(bytes.size()), &got,
                        nullptr) &&
        got >= sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
      bytes.resize(got);
      const auto *descriptor =
          reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR *>(bytes.data());
      auto model = field(bytes, descriptor->VendorIdOffset);
      const auto product = field(bytes, descriptor->ProductIdOffset);
      if (!product.empty())
        model += (model.empty() ? "" : " ") + product;
      if (!model.empty())
        info.modelNumber = model;
      info.serialNumber = field(bytes, descriptor->SerialNumberOffset);
      info.firmwareRevision = field(bytes, descriptor->ProductRevisionOffset);
      info.isRemovable = descriptor->RemovableMedia != FALSE;
      if (descriptor->BusType == BusTypeUsb) {
        info.busType = disk::BusType::USB;
        info.busTypeString = "USB";
      } else if (descriptor->BusType == BusTypeSata) {
        info.busType = disk::BusType::SATA;
        info.busTypeString = "SATA";
      }
    }
  }
  return info;
}
std::vector<disk::DeviceInformation> WindowsDiskEnumerator::enumerateDisks() {
  const auto devices =
      SetupDiGetClassDevsW(&GUID_DEVINTERFACE_DISK, nullptr, nullptr,
                           DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (devices == INVALID_HANDLE_VALUE)
    throw disk::DiskException("Cannot enumerate disk interfaces",
                              GetLastError());
  struct Cleanup {
    HDEVINFO value;
    ~Cleanup() { SetupDiDestroyDeviceInfoList(value); }
  } cleanup{devices};
  std::set<DWORD> numbers;
  for (DWORD i = 0;; ++i) {
    SP_DEVICE_INTERFACE_DATA deviceInterface{};
    deviceInterface.cbSize = sizeof(deviceInterface);
    if (!SetupDiEnumDeviceInterfaces(devices, nullptr, &GUID_DEVINTERFACE_DISK,
                                     i, &deviceInterface)) {
      if (GetLastError() == ERROR_NO_MORE_ITEMS)
        break;
      throw disk::DiskException("Disk interface enumeration failed",
                                GetLastError());
    }
    DWORD size{};
    SetupDiGetDeviceInterfaceDetailW(devices, &deviceInterface, nullptr, 0,
                                     &size, nullptr);
    if (size < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || size > 65536)
      continue;
    std::vector<uint8_t> buffer(size);
    auto *detail =
        reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(buffer.data());
    detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    if (!SetupDiGetDeviceInterfaceDetailW(devices, &deviceInterface, detail,
                                          size, nullptr, nullptr))
      continue;
    Handle h{CreateFileW(detail->DevicePath, 0,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         nullptr, OPEN_EXISTING, 0, nullptr)};
    STORAGE_DEVICE_NUMBER number{};
    DWORD got{};
    if (h.value != INVALID_HANDLE_VALUE &&
        DeviceIoControl(h.value, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0,
                        &number, sizeof(number), &got, nullptr) &&
        number.DeviceType == FILE_DEVICE_DISK)
      numbers.insert(number.DeviceNumber);
  }
  std::vector<disk::DeviceInformation> result;
  for (const auto n : numbers)
    result.push_back(getDiskInfo(n));
  return result;
}
} // namespace recoverysuite::platform::windows
