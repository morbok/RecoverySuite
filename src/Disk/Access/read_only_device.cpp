#include "read_only_device.h"
#include "path_utf8.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <set>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#endif
#endif
namespace recoverysuite::disk {
struct ReadOnlyDevice::Impl {
  uint64_t size = 0, identity = 0, sequence = 0;
  uint32_t sector = 0;
  std::string path;
  mutable std::mutex mutex;
#ifdef _WIN32
  HANDLE handle = INVALID_HANDLE_VALUE;
  DWORD number = 0;
  ~Impl() {
    if (handle != INVALID_HANDLE_VALUE)
      CloseHandle(handle);
  }
#else
  int handle = -1;
  ~Impl() {
    if (handle >= 0)
      ::close(handle);
  }
#endif
};
#ifdef __linux__
namespace {
std::set<uint64_t> dependencies(uint64_t device) {
  std::set<uint64_t> result;
  std::vector<uint64_t> queue{device};
  for (size_t i = 0; i < queue.size(); ++i) {
    if (result.size() > 128)
      throw DiskException("Block dependency graph exceeds limits");
    const auto id = queue[i];
    if (!result.insert(id).second)
      continue;
    const auto node = std::filesystem::path("/sys/dev/block") /
                      (std::to_string(major(dev_t(id))) + ":" +
                       std::to_string(minor(dev_t(id))));
    std::error_code error;
    const auto canonical = std::filesystem::canonical(node, error);
    if (error)
      continue;
    const auto add = [&](const std::filesystem::path &path) {
      std::ifstream f(path / "dev");
      unsigned a{}, b{};
      char colon{};
      if (f >> a >> colon >> b && colon == ':')
        queue.push_back(uint64_t(makedev(a, b)));
    };
    if (std::filesystem::exists(canonical / "partition"))
      add(canonical.parent_path());
    if (std::filesystem::is_directory(canonical / "slaves"))
      for (const auto &slave :
           std::filesystem::directory_iterator(canonical / "slaves"))
        add(slave.path());
  }
  return result;
}
bool overlap(const std::set<uint64_t> &a, const std::set<uint64_t> &b) {
  for (const auto value : a)
    if (b.contains(value))
      return true;
  return false;
}
void unmounted(uint64_t device) {
  const auto source = dependencies(device);
  std::ifstream mounts("/proc/self/mountinfo");
  if (!mounts)
    throw DiskException("Cannot verify source mount state");
  std::string line;
  while (std::getline(mounts, line)) {
    std::istringstream row(line);
    std::string first, second, id;
    row >> first >> second >> id;
    const auto colon = id.find(':');
    if (colon == std::string::npos)
      continue;
    try {
      const auto a = std::stoul(id.substr(0, colon)),
                 b = std::stoul(id.substr(colon + 1));
      if (overlap(source, dependencies(uint64_t(makedev(a, b)))))
        throw AccessDeniedException(
            "Unmount all volumes on the source physical device before capture");
    } catch (const std::invalid_argument &) {
      throw DiskException("Cannot parse mounted device identity");
    }
  }
}
} // namespace
#endif
ReadOnlyDevice::ReadOnlyDevice() : impl_(std::make_unique<Impl>()) {}
ReadOnlyDevice::~ReadOnlyDevice() = default;
void ReadOnlyDevice::open(const std::string &path) { openImpl(path, -1); }
void ReadOnlyDevice::openAuthorized(const std::string &path, int descriptor) {
  if (descriptor < 0)
    throw InvalidParameterException("Invalid authorized descriptor");
  openImpl(path, descriptor);
}
void ReadOnlyDevice::openImpl(const std::string &path,
                              int authorizedDescriptor) {
  if (path.empty() || path.find(char{0}) != std::string::npos)
    throw InvalidParameterException("Invalid device path");
  auto candidate = std::make_unique<Impl>();
  candidate->path = path;
#ifdef _WIN32
  if (authorizedDescriptor >= 0)
    throw InvalidParameterException(
        "Unix descriptors are unavailable on Windows");
  const std::string prefix = "\\\\.\\PhysicalDrive";
  if (!path.starts_with(prefix) || path.size() == prefix.size() ||
      path.substr(prefix.size()).find_first_not_of("0123456789") !=
          std::string::npos)
    throw InvalidParameterException(
        "Use an explicit Windows PhysicalDrive path");
  candidate->handle =
      CreateFileW(nativePath(path).wstring().c_str(), GENERIC_READ,
                  FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (candidate->handle == INVALID_HANDLE_VALUE)
    throw AccessDeniedException("Cannot open physical disk read-only; "
                                "administrator access may be required",
                                GetLastError());
  DWORD got{};
  DISK_GEOMETRY geometry{};
  GET_LENGTH_INFORMATION length{};
  STORAGE_DEVICE_NUMBER number{};
  GET_DISK_ATTRIBUTES attributes{};
  if (!DeviceIoControl(candidate->handle, IOCTL_STORAGE_GET_DEVICE_NUMBER,
                       nullptr, 0, &number, sizeof(number), &got, nullptr) ||
      number.DeviceType != FILE_DEVICE_DISK ||
      !DeviceIoControl(candidate->handle, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                       nullptr, 0, &geometry, sizeof(geometry), &got,
                       nullptr) ||
      !DeviceIoControl(candidate->handle, IOCTL_DISK_GET_LENGTH_INFO, nullptr,
                       0, &length, sizeof(length), &got, nullptr) ||
      length.Length.QuadPart <= 0)
    throw DiskException("Cannot query device geometry", GetLastError());
  if (!DeviceIoControl(candidate->handle, IOCTL_DISK_GET_DISK_ATTRIBUTES,
                       nullptr, 0, &attributes, sizeof(attributes), &got,
                       nullptr) ||
      !(attributes.Attributes & DISK_ATTRIBUTE_OFFLINE))
    throw AccessDeniedException("Source disk must be offline before capture; "
                                "its state is never changed automatically");
  candidate->number = number.DeviceNumber;
  candidate->sector = geometry.BytesPerSector;
  candidate->size = uint64_t(length.Length.QuadPart);
#elif defined(__linux__)
  if (authorizedDescriptor >= 0) {
    const auto flags = ::fcntl(authorizedDescriptor, F_GETFL);
    if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY)
      throw AccessDeniedException("Authorized source handle must be read-only");
    candidate->handle = ::fcntl(authorizedDescriptor, F_DUPFD_CLOEXEC, 0);
  } else {
    candidate->handle = ::open(path.c_str(), O_RDONLY | O_EXCL | O_NOFOLLOW |
                                                 O_NONBLOCK | O_CLOEXEC);
  }
  if (candidate->handle < 0)
    throw AccessDeniedException("Cannot open block device read-only (missing, "
                                "busy, or permission denied)",
                                errno);
  struct stat info{}, named{};
  int sector = 0;
  if (::fstat(candidate->handle, &info) || !S_ISBLK(info.st_mode) ||
      ::lstat(path.c_str(), &named) || !S_ISBLK(named.st_mode) ||
      named.st_rdev != info.st_rdev ||
      ::ioctl(candidate->handle, BLKSSZGET, &sector) ||
      ::ioctl(candidate->handle, BLKGETSIZE64, &candidate->size))
    throw DiskException(
        "Source is not a readable block device with known geometry");
  candidate->sector = uint32_t(sector);
  candidate->identity = uint64_t(info.st_rdev);
  candidate->path = utf8Path(std::filesystem::canonical(nativePath(path)));
  unmounted(candidate->identity);
#ifdef BLKGETDISKSEQ
  (void)::ioctl(candidate->handle, BLKGETDISKSEQ, &candidate->sequence);
#endif
#else
  throw DiskException(
      "Native device capture is supported on Linux and Windows only");
#endif
  if (candidate->sector < 512 || candidate->sector > 4096 ||
      (candidate->sector & (candidate->sector - 1)) || !candidate->size ||
      candidate->size > uint64_t(std::numeric_limits<int64_t>::max()) ||
      candidate->size % candidate->sector)
    throw DiskException("Unsupported physical sector geometry");
  impl_ = std::move(candidate);
}
void ReadOnlyDevice::close() { impl_ = std::make_unique<Impl>(); }
bool ReadOnlyDevice::isOpen() const noexcept {
#ifdef _WIN32
  return impl_->handle != INVALID_HANDLE_VALUE;
#else
  return impl_->handle >= 0;
#endif
}
uint32_t ReadOnlyDevice::getSectorSize() const noexcept {
  return impl_->sector;
}
uint64_t ReadOnlyDevice::getTotalSectors() const noexcept {
  return impl_->sector ? impl_->size / impl_->sector : 0;
}
DeviceInformation ReadOnlyDevice::getDiskInfo() const {
  validateIdentity();
  DeviceInformation info;
  info.devicePath = impl_->path;
  info.isReadOnly = true;
  info.modelNumber = "Unknown";
#ifdef _WIN32
  info.diskNumber = impl_->number;
#elif defined(__linux__)
  info.diskNumber = (uint64_t(major(dev_t(impl_->identity))) << 32) |
                    minor(dev_t(impl_->identity));
#endif
  info.bytesPerSector = impl_->sector;
  info.logicalBytesPerSector = impl_->sector;
  info.totalSizeBytes = impl_->size;
  info.totalSectors = getTotalSectors();
  return info;
}
void ReadOnlyDevice::validateIdentity() const {
  if (!isOpen())
    throw DiskException("Device is closed");
#ifdef _WIN32
  DWORD got{};
  GET_LENGTH_INFORMATION length{};
  GET_DISK_ATTRIBUTES attributes{};
  STORAGE_DEVICE_NUMBER number{};
  if (!DeviceIoControl(impl_->handle, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0,
                       &length, sizeof(length), &got, nullptr) ||
      length.Length.QuadPart < 0 ||
      uint64_t(length.Length.QuadPart) != impl_->size ||
      !DeviceIoControl(impl_->handle, IOCTL_DISK_GET_DISK_ATTRIBUTES, nullptr,
                       0, &attributes, sizeof(attributes), &got, nullptr) ||
      !(attributes.Attributes & DISK_ATTRIBUTE_OFFLINE) ||
      !DeviceIoControl(impl_->handle, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr,
                       0, &number, sizeof(number), &got, nullptr) ||
      number.DeviceNumber != impl_->number)
    throw DiskException("Physical source changed or became online");
#elif defined(__linux__)
  struct stat info{}, named{};
  uint64_t size = 0;
  if (::fstat(impl_->handle, &info) || ::lstat(impl_->path.c_str(), &named) ||
      !S_ISBLK(named.st_mode) || uint64_t(info.st_rdev) != impl_->identity ||
      info.st_rdev != named.st_rdev ||
      ::ioctl(impl_->handle, BLKGETSIZE64, &size) || size != impl_->size)
    throw DiskException("Physical source was replaced or resized");
#ifdef BLKGETDISKSEQ
  uint64_t sequence = 0;
  if (impl_->sequence && (::ioctl(impl_->handle, BLKGETDISKSEQ, &sequence) ||
                          sequence != impl_->sequence))
    throw DiskException("Physical source media changed");
#endif
#endif
}
void ReadOnlyDevice::validateDestination(const std::string &directory) const {
  validateIdentity();
#ifdef _WIN32
  const auto path = std::filesystem::absolute(nativePath(directory));
  const auto root = path.root_path().wstring();
  if (root.size() != 3 || root[1] != L':')
    throw InvalidParameterException("Destination must be on a local disk");
  const auto volume = L"\\\\.\\" + root.substr(0, 2);
  HANDLE handle = CreateFileW(
      volume.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr, OPEN_EXISTING, 0, nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    throw DiskException("Cannot verify destination device");
  std::vector<uint8_t> buffer(65536);
  DWORD got{};
  const bool ok = DeviceIoControl(handle, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                                  nullptr, 0, buffer.data(),
                                  DWORD(buffer.size()), &got, nullptr) != FALSE;
  CloseHandle(handle);
  if (!ok || got < sizeof(VOLUME_DISK_EXTENTS))
    throw DiskException("Cannot verify destination volume extents");
  const auto *extents =
      reinterpret_cast<const VOLUME_DISK_EXTENTS *>(buffer.data());
  if (extents->NumberOfDiskExtents >
      (got - FIELD_OFFSET(VOLUME_DISK_EXTENTS, Extents)) / sizeof(DISK_EXTENT))
    throw DiskException("Invalid destination volume extent count");
  for (DWORD i = 0; i < extents->NumberOfDiskExtents; ++i)
    if (extents->Extents[i].DiskNumber == impl_->number)
      throw AccessDeniedException("Capture destination is on the source disk");
#elif defined(__linux__)
  struct stat output{};
  if (::stat(directory.c_str(), &output) || !S_ISDIR(output.st_mode))
    throw DiskException("Cannot inspect capture destination");
  if (overlap(dependencies(impl_->identity),
              dependencies(uint64_t(output.st_dev))))
    throw AccessDeniedException(
        "Capture destination is on the source physical device");
#endif
}
bool ReadOnlyDevice::readSectors(uint64_t start, uint64_t count,
                                 std::vector<std::byte> &bytes) {
  std::lock_guard lock(impl_->mutex);
  bytes.clear();
  validateIdentity();
  if (!count || start >= getTotalSectors() ||
      count > getTotalSectors() - start || count > 65536 / impl_->sector)
    return false;
  const auto length = size_t(count * impl_->sector),
             offset = start * impl_->sector;
  std::vector<std::byte> result(length);
#ifdef _WIN32
  LARGE_INTEGER position{};
  position.QuadPart = LONGLONG(offset);
  DWORD got{};
  if (!SetFilePointerEx(impl_->handle, position, nullptr, FILE_BEGIN) ||
      !ReadFile(impl_->handle, result.data(), DWORD(length), &got, nullptr) ||
      got != length)
    throw MediaException("Physical source read failed or was short");
#elif defined(__linux__)
  size_t received = 0;
  while (received < length) {
    const auto n = ::pread(impl_->handle, result.data() + received,
                           length - received, off_t(offset + received));
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      throw MediaException("Physical source read failed or was short");
    received += size_t(n);
  }
#else
  return false;
#endif
  bytes = std::move(result);
  return true;
}
bool ReadOnlyDevice::writeSectors(uint64_t, uint64_t,
                                  const std::vector<std::byte> &) {
  throw AccessDeniedException("Physical source writes are disabled");
}
} // namespace recoverysuite::disk
