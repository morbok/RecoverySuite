#include "disk_image_reader.h"
#include "path_utf8.h"
#include <filesystem>
#include <limits>
#include <sys/stat.h>
#ifdef _WIN32
#include <algorithm>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
namespace recoverysuite::disk {
DiskImageReader::DiskImageReader(uint32_t sectorSize)
    : sectorSize_(sectorSize) {
  if (sectorSize < 512 || sectorSize > 4096 || (sectorSize & (sectorSize - 1)))
    throw InvalidParameterException("Unsupported image sector size");
}
DiskImageReader::~DiskImageReader() = default;
void DiskImageReader::open(const std::string &path) {
  std::lock_guard lock(mutex_);
  if (path.empty() || path.find(char{0}) != std::string::npos)
    throw InvalidParameterException("Invalid image path");
  std::unique_ptr<std::FILE, FileCloser> candidate;
  uint64_t size = 0;
#ifdef _WIN32
  const auto native = nativePath(path).wstring();
  HANDLE handle = CreateFileW(
      native.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    throw DiskException(
        "Cannot open a regular read-only image (access denied or source busy)");
  FILE_ATTRIBUTE_TAG_INFO tag{};
  FILE_STANDARD_INFO standard{};
  FILE_ID_INFO id{};
  FILE_BASIC_INFO basic{};
  if (GetFileType(handle) != FILE_TYPE_DISK ||
      !GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag,
                                    sizeof(tag)) ||
      (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
      !GetFileInformationByHandleEx(handle, FileStandardInfo, &standard,
                                    sizeof(standard)) ||
      standard.Directory || standard.EndOfFile.QuadPart <= 0 ||
      !GetFileInformationByHandleEx(handle, FileIdInfo, &id, sizeof(id)) ||
      !GetFileInformationByHandleEx(handle, FileBasicInfo, &basic,
                                    sizeof(basic))) {
    CloseHandle(handle);
    throw DiskException(
        "Source must be a regular image without reparse points");
  }
  const auto descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(handle),
                                          _O_RDONLY | _O_BINARY);
  if (descriptor < 0) {
    CloseHandle(handle);
    throw DiskException("Cannot retain image handle");
  }
  auto *stream = _fdopen(descriptor, "rb");
  if (!stream) {
    _close(descriptor);
    throw DiskException("Cannot open image stream");
  }
  candidate.reset(stream);
  size = uint64_t(standard.EndOfFile.QuadPart);
  device_ = id.VolumeSerialNumber;
  std::copy(std::begin(id.FileId.Identifier), std::end(id.FileId.Identifier),
            fileId_.begin());
  modifiedSeconds_ = uint64_t(basic.LastWriteTime.QuadPart);

#else
  const int descriptor =
      ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
  if (descriptor < 0)
    throw DiskException("Cannot open disk image for reading");
  struct stat info{};
  if (::fstat(descriptor, &info) || !S_ISREG(info.st_mode) ||
      info.st_size <= 0) {
    ::close(descriptor);
    throw DiskException("Source must be a nonempty regular disk image");
  }
  auto *stream = ::fdopen(descriptor, "rb");
  if (!stream) {
    ::close(descriptor);
    throw DiskException("Cannot open image stream");
  }
  candidate.reset(stream);
  size = static_cast<uint64_t>(info.st_size);
#endif
  if (!size || size % sectorSize_)
    throw DiskException(
        "Image size must be a positive multiple of its sector size");
  const auto canonical = utf8Path(std::filesystem::canonical(nativePath(path)));
#ifndef _WIN32
  device_ = static_cast<uint64_t>(info.st_dev);
  inode_ = static_cast<uint64_t>(info.st_ino);
#ifdef __APPLE__
  modifiedSeconds_ = static_cast<uint64_t>(info.st_mtimespec.tv_sec);
  modifiedNanos_ = static_cast<uint64_t>(info.st_mtimespec.tv_nsec);
#else
  modifiedSeconds_ = static_cast<uint64_t>(info.st_mtim.tv_sec);
  modifiedNanos_ = static_cast<uint64_t>(info.st_mtim.tv_nsec);
#endif
#endif
  file_ = std::move(candidate);
  size_ = size;
  path_ = canonical;
}
void DiskImageReader::close() {
  std::lock_guard lock(mutex_);
  file_.reset();
  size_ = 0;
  path_.clear();
}
bool DiskImageReader::isOpen() const noexcept { return bool(file_); }
uint32_t DiskImageReader::getSectorSize() const noexcept { return sectorSize_; }
uint64_t DiskImageReader::getTotalSectors() const noexcept {
  return size_ / sectorSize_;
}
const std::string &DiskImageReader::sourcePath() const noexcept {
  return path_;
}
DeviceInformation DiskImageReader::getDiskInfo() const {
  validateIdentity();
  DeviceInformation info{};
  info.isReadOnly = true;
  info.modelNumber = "Disk image";
  info.totalSectors = getTotalSectors();
  info.bytesPerSector = sectorSize_;
  info.logicalBytesPerSector = sectorSize_;
  info.physicalBytesPerSector = sectorSize_;
  info.totalSizeBytes = size_;
  return info;
}
void DiskImageReader::validateIdentity() const {
  if (!file_)
    throw DiskException("Image is closed");
#ifdef _WIN32
  const auto handle =
      reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file_.get())));
  FILE_ID_INFO opened{};
  FILE_BASIC_INFO basic{};
  FILE_STANDARD_INFO standard{};
  if (!GetFileInformationByHandleEx(handle, FileIdInfo, &opened,
                                    sizeof(opened)) ||
      !GetFileInformationByHandleEx(handle, FileBasicInfo, &basic,
                                    sizeof(basic)) ||
      !GetFileInformationByHandleEx(handle, FileStandardInfo, &standard,
                                    sizeof(standard)) ||
      opened.VolumeSerialNumber != device_ ||
      !std::equal(std::begin(opened.FileId.Identifier),
                  std::end(opened.FileId.Identifier), fileId_.begin()) ||
      standard.EndOfFile.QuadPart < 0 ||
      uint64_t(standard.EndOfFile.QuadPart) != size_ ||
      uint64_t(basic.LastWriteTime.QuadPart) != modifiedSeconds_)
    throw DiskException("Source image changed; reopen it before scanning");
  const auto native = nativePath(path_).wstring();
  HANDLE current = CreateFileW(
      native.c_str(), FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  FILE_ID_INFO named{};
  FILE_ATTRIBUTE_TAG_INFO tag{};
  const bool valid =
      current != INVALID_HANDLE_VALUE &&
      GetFileInformationByHandleEx(current, FileIdInfo, &named,
                                   sizeof(named)) &&
      GetFileInformationByHandleEx(current, FileAttributeTagInfo, &tag,
                                   sizeof(tag)) &&
      !(tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
      named.VolumeSerialNumber == device_ &&
      std::equal(std::begin(named.FileId.Identifier),
                 std::end(named.FileId.Identifier), fileId_.begin());
  if (current != INVALID_HANDLE_VALUE)
    CloseHandle(current);
  if (!valid)
    throw DiskException("Source image was replaced");
#else
  struct stat opened{}, current{};
  if (::fstat(::fileno(file_.get()), &opened) ||
      ::stat(path_.c_str(), &current) ||
      static_cast<uint64_t>(opened.st_dev) != device_ ||
      static_cast<uint64_t>(opened.st_ino) != inode_ ||
      current.st_dev != opened.st_dev || current.st_ino != opened.st_ino ||
      static_cast<uint64_t>(opened.st_size) != size_)
    throw DiskException("Source image changed; reopen it before scanning");
#ifdef __APPLE__
  const auto modified = opened.st_mtimespec;
#else
  const auto modified = opened.st_mtim;
#endif
  if (static_cast<uint64_t>(modified.tv_sec) != modifiedSeconds_ ||
      static_cast<uint64_t>(modified.tv_nsec) != modifiedNanos_)
    throw DiskException("Source image was modified; reopen it before scanning");
#endif
}
bool DiskImageReader::writeSectors(uint64_t, uint64_t,
                                   const std::vector<std::byte> &) {
  throw AccessDeniedException("Disk images are opened read-only");
}
bool DiskImageReader::readSectors(uint64_t start, uint64_t count,
                                  std::vector<std::byte> &buffer) {
  std::lock_guard lock(mutex_);
  buffer.clear();
  validateIdentity();
  constexpr uint64_t kMaximumRead = 64 * 1024 * 1024;
  const auto total = getTotalSectors();
  if (!count || start >= total || count > total - start ||
      count > kMaximumRead / sectorSize_)
    return false;
  const uint64_t offset = start * sectorSize_;
#ifdef _WIN32
  if (_fseeki64(file_.get(), static_cast<int64_t>(offset), SEEK_SET))
    throw DiskException("Image seek failed");
#else
  if (offset > static_cast<uint64_t>(std::numeric_limits<off_t>::max()) ||
      ::fseeko(file_.get(), static_cast<off_t>(offset), SEEK_SET))
    throw DiskException("Image seek failed");
#endif
  std::vector<std::byte> data(static_cast<size_t>(count * sectorSize_));
  if (std::fread(data.data(), 1, data.size(), file_.get()) != data.size())
    throw DiskException("Image changed or a short read occurred");
  buffer = std::move(data);
  return true;
}
} // namespace recoverysuite::disk
