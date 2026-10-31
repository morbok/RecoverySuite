#include "safe_file_exporter.h"
#include "Disk/Access/path_utf8.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <filesystem>
#include <limits>
#include <random>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#include <winternl.h>
#endif
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace recoverysuite::application::service {
#ifndef _WIN32
namespace {
class Descriptor {
public:
  explicit Descriptor(int value = -1) : value_(value) {}
  ~Descriptor() {
    if (value_ >= 0)
      ::close(value_);
  }
  Descriptor(const Descriptor &) = delete;
  Descriptor &operator=(const Descriptor &) = delete;
  int get() const { return value_; }
  void reset(int value) {
    if (value_ >= 0)
      ::close(value_);
    value_ = value;
  }

private:
  int value_;
};
void checkCancel(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled())
    throw std::runtime_error("Output cancelled");
}
} // namespace
struct SafeFileExporter::Impl {
  Descriptor directory;
};
SafeFileExporter::SafeFileExporter(const std::string &directory)
    : impl_(std::make_unique<Impl>()) {
  if (directory.empty() || directory.find(char{0}) != std::string::npos)
    throw std::invalid_argument("Choose an existing output directory");
  const auto input = std::filesystem::path(directory);
  for (const auto &component : input) {
    if (component == "..")
      throw std::invalid_argument(
          "Output directory must not contain parent traversal");
  }
  const auto path = std::filesystem::absolute(input).lexically_normal();
  impl_->directory.reset(::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (impl_->directory.get() < 0)
    throw std::runtime_error("Cannot open output directory");
  for (const auto &part : path.relative_path()) {
    if (part.empty() || part == ".")
      continue;
    const int next = ::openat(impl_->directory.get(), part.c_str(),
                              O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (next < 0)
      throw std::runtime_error(
          "Output directory is missing, inaccessible or contains a symlink");
    impl_->directory.reset(next);
  }
}
SafeFileExporter::~SafeFileExporter() = default;
namespace {
void writeAt(int directory, const std::string &name, uint64_t size,
             const SafeFileExporter::ReadAt &read, bool verify,
             const std::function<bool()> &cancelled) {
  if (name.empty() || name.size() > 255 || name == "." || name == ".." ||
      name.find_first_of("/\\:") != std::string::npos ||
      std::any_of(name.begin(), name.end(),
                  [](unsigned char c) { return c < 32 || c == 127; }))
    throw std::invalid_argument("Unsafe recovered filename");
  checkCancel(cancelled);
  Descriptor output(::openat(directory, name.c_str(),
                             O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
                             0600));
  if (output.get() < 0)
    throw std::runtime_error("Cannot create output file; it may already exist");
  bool completed = false;
  // Remove only this operation's incomplete inode; never unlink a replacement
  // or previous file.
  const auto cleanup = [&] {
    struct stat opened{}, current{};
    if (!completed && !::fstat(output.get(), &opened) &&
        !::fstatat(directory, name.c_str(), &current, AT_SYMLINK_NOFOLLOW) &&
        opened.st_dev == current.st_dev && opened.st_ino == current.st_ino)
      (void)::unlinkat(directory, name.c_str(), 0);
  };
  try {
    if (size > uint64_t(std::numeric_limits<off_t>::max()))
      throw std::runtime_error("Output exceeds platform offset limit");
    for (uint64_t offset = 0; offset < size;) {
      checkCancel(cancelled);
      const auto length =
          static_cast<size_t>(std::min<uint64_t>(65536, size - offset));
      const auto chunk = read(offset, length);
      if (chunk.size() != length)
        throw std::runtime_error("Incomplete output source chunk");
      for (size_t written = 0; written < length;) {
        checkCancel(cancelled);
        const auto n =
            ::write(output.get(), chunk.data() + written, length - written);
        if (n < 0 && errno == EINTR)
          continue;
        if (n <= 0)
          throw std::runtime_error("Output write failed");
        written += static_cast<size_t>(n);
      }
      offset += length;
    }
    if (::fsync(output.get()))
      throw std::runtime_error("Output flush failed");
    if (verify)
      for (uint64_t offset = 0; offset < size;) {
        checkCancel(cancelled);
        const auto length =
            static_cast<size_t>(std::min<uint64_t>(65536, size - offset));
        const auto expected = read(offset, length);
        if (expected.size() != length)
          throw std::runtime_error("Incomplete verification source chunk");
        std::vector<uint8_t> actual(length);
        size_t received = 0;
        while (received < length) {
          checkCancel(cancelled);
          const auto n =
              ::pread(output.get(), actual.data() + received, length - received,
                      static_cast<off_t>(offset + received));
          if (n < 0 && errno == EINTR)
            continue;
          if (n <= 0)
            throw std::runtime_error("Output verification read failed");
          received += static_cast<size_t>(n);
        }
        if (actual != expected)
          throw std::runtime_error("Output verification failed");
        offset += length;
      }
    checkCancel(cancelled);
    struct stat opened{}, named{};
    if (::fstat(output.get(), &opened) ||
        ::fstatat(directory, name.c_str(), &named, AT_SYMLINK_NOFOLLOW) ||
        opened.st_dev != named.st_dev || opened.st_ino != named.st_ino ||
        opened.st_size < 0 || static_cast<uint64_t>(opened.st_size) != size)
      throw std::runtime_error("Output file changed before completion");
    if (::fsync(directory))
      throw std::runtime_error("Output directory flush failed");
    completed = true;
  } catch (const std::exception &) {
    cleanup();
    throw;
  }
}
} // namespace
void SafeFileExporter::write(const std::string &name,
                             const std::vector<uint8_t> &data, bool verify,
                             const std::function<bool()> &cancelled) {
  writeStream(
      name, data.size(),
      [&](uint64_t offset, size_t length) {
        return std::vector<uint8_t>(data.begin() + offset,
                                    data.begin() + offset + length);
      },
      verify, false, cancelled);
}
void SafeFileExporter::writeTree(const std::string &path,
                                 const std::vector<uint8_t> &data, bool verify,
                                 const std::function<bool()> &cancelled) {
  writeStream(
      path, data.size(),
      [&](uint64_t offset, size_t length) {
        return std::vector<uint8_t>(data.begin() + offset,
                                    data.begin() + offset + length);
      },
      verify, true, cancelled);
}
void SafeFileExporter::writeStream(const std::string &path, uint64_t size,
                                   const ReadAt &read, bool verify, bool tree,
                                   const std::function<bool()> &cancelled) {
  if (!tree) {
    writeAt(impl_->directory.get(), path, size, read, verify, cancelled);
    return;
  }
  if (path.empty() || path.size() > 4095)
    throw std::invalid_argument("Invalid recovered relative path");
  std::vector<std::string> components;
  for (size_t start = 0; start <= path.size();) {
    const auto end = path.find('/', start);
    const auto part =
        path.substr(start, end == std::string::npos ? end : end - start);
    if (part.empty() || part.size() > 255 || part == "." || part == ".." ||
        part.find_first_of("\\:") != std::string::npos ||
        std::any_of(part.begin(), part.end(),
                    [](unsigned char c) { return c < 32 || c == 127; }))
      throw std::invalid_argument("Unsafe recovered path component");
    components.push_back(part);
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  if (components.size() > 33)
    throw std::invalid_argument("Recovered directory depth exceeds 32");
  checkCancel(cancelled);
  Descriptor parent(::fcntl(impl_->directory.get(), F_DUPFD_CLOEXEC, 0));
  if (parent.get() < 0)
    throw std::runtime_error("Cannot anchor recovered directory");
  for (size_t i = 0; i + 1 < components.size(); ++i) {
    checkCancel(cancelled);
    if (::mkdirat(parent.get(), components[i].c_str(), 0700) && errno != EEXIST)
      throw std::runtime_error("Cannot create recovered directory");
    const int next = ::openat(parent.get(), components[i].c_str(),
                              O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (next < 0)
      throw std::runtime_error(
          "Recovered directory is inaccessible or contains a symlink");
    Descriptor child(next);
    if (::fsync(parent.get()))
      throw std::runtime_error("Cannot flush recovered parent directory");
    parent.reset(::fcntl(child.get(), F_DUPFD_CLOEXEC, 0));
    if (parent.get() < 0)
      throw std::runtime_error("Cannot anchor recovered child directory");
  }
  writeAt(parent.get(), components.back(), size, read, verify, cancelled);
}
void SafeFileExporter::verifyStream(const std::string &path, uint64_t size,
                                    const ReadAt &read, bool tree,
                                    const std::function<bool()> &cancelled) {
  if (path.empty() || path.size() > 4095 ||
      (!tree && path.find('/') != std::string::npos))
    throw std::invalid_argument("Invalid saved output path");
  Descriptor parent(::fcntl(impl_->directory.get(), F_DUPFD_CLOEXEC, 0));
  if (parent.get() < 0)
    throw std::runtime_error("Cannot anchor saved output");
  size_t start = 0, depth = 0;
  while (true) {
    checkCancel(cancelled);
    const auto end = path.find('/', start), length = end == std::string::npos
                                                         ? path.size() - start
                                                         : end - start;
    const auto part = path.substr(start, length);
    if (part.empty() || part.size() > 255 || part == "." || part == ".." ||
        part.find_first_of("\\:") != std::string::npos ||
        std::any_of(part.begin(), part.end(),
                    [](unsigned char c) { return c < 32 || c == 127; }))
      throw std::invalid_argument("Unsafe saved output path");
    if (end == std::string::npos) {
      Descriptor file(::openat(parent.get(), part.c_str(),
                               O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
      struct stat info{};
      if (file.get() < 0 || ::fstat(file.get(), &info) ||
          !S_ISREG(info.st_mode) || info.st_size < 0 ||
          uint64_t(info.st_size) != size)
        throw std::runtime_error("Saved output is missing or changed");
      for (uint64_t offset = 0; offset < size;) {
        checkCancel(cancelled);
        const auto n =
            static_cast<size_t>(std::min<uint64_t>(65536, size - offset));
        const auto expected = read(offset, n);
        std::vector<uint8_t> actual(n);
        size_t got = 0;
        while (got < n) {
          const auto count = ::pread(file.get(), actual.data() + got, n - got,
                                     static_cast<off_t>(offset + got));
          if (count < 0 && errno == EINTR)
            continue;
          if (count <= 0)
            throw std::runtime_error("Saved output read failed");
          got += static_cast<size_t>(count);
        }
        if (expected.size() != n || actual != expected)
          throw std::runtime_error("Saved output bytes changed");
        offset += n;
      }
      struct stat after{}, named{};
      if (::fstat(file.get(), &after) ||
          ::fstatat(parent.get(), part.c_str(), &named, AT_SYMLINK_NOFOLLOW) ||
          after.st_dev != named.st_dev || after.st_ino != named.st_ino ||
          after.st_size != info.st_size)
        throw std::runtime_error("Saved output changed during verification");
      return;
    }
    if (++depth > 32)
      throw std::invalid_argument("Saved output depth exceeds 32");
    const int next = ::openat(parent.get(), part.c_str(),
                              O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (next < 0)
      throw std::runtime_error(
          "Saved output folder is missing or contains a symlink");
    parent.reset(next);
    start = end + 1;
  }
}
void SafeFileExporter::replaceState(const std::string &name,
                                    const std::vector<uint8_t> &data) {
  if (name.empty() || name.size() > 255 || name == "." || name == ".." ||
      name.find_first_of("/\\:") != std::string::npos ||
      std::any_of(name.begin(), name.end(),
                  [](unsigned char c) { return c < 32 || c == 127; }))
    throw std::invalid_argument("Unsafe session filename");
  static std::atomic_uint64_t sequence{0};
  const auto temporary = ".rs-state-" + std::to_string(std::random_device{}()) +
                         "-" + std::to_string(++sequence);
  writeAt(impl_->directory.get(), temporary, data.size(),
          [&](uint64_t offset, size_t length) {
            return std::vector<uint8_t>(data.begin() + offset,
                                        data.begin() + offset + length);
          },
          true, {});
  if (::renameat(impl_->directory.get(), temporary.c_str(),
                 impl_->directory.get(), name.c_str())) {
    (void)::unlinkat(impl_->directory.get(), temporary.c_str(), 0);
    throw std::runtime_error("Cannot commit session checkpoint");
  }
  if (::fsync(impl_->directory.get()))
    throw std::runtime_error("Cannot flush session checkpoint directory");
}

#else
namespace {
class WinHandle {
public:
  HANDLE value = INVALID_HANDLE_VALUE;
  explicit WinHandle(HANDLE h = INVALID_HANDLE_VALUE) : value(h) {}
  ~WinHandle() {
    if (value != INVALID_HANDLE_VALUE && value)
      CloseHandle(value);
  }
  WinHandle(const WinHandle &) = delete;
  WinHandle &operator=(const WinHandle &) = delete;
  WinHandle(WinHandle &&other) noexcept : value(other.value) {
    other.value = INVALID_HANDLE_VALUE;
  }
  WinHandle &operator=(WinHandle &&other) noexcept {
    if (this != &other) {
      if (value != INVALID_HANDLE_VALUE && value)
        CloseHandle(value);
      value = other.value;
      other.value = INVALID_HANDLE_VALUE;
    }
    return *this;
  }
};
void winCancel(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled())
    throw std::runtime_error("Output cancelled");
}
std::wstring wide(const std::string &text) {
  if (text.size() > 32767)
    throw std::invalid_argument("UTF-8 path too long");
  const auto n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                     int(text.size()), nullptr, 0);
  if (!n)
    throw std::invalid_argument("Invalid UTF-8 filename");
  std::wstring result(n, L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                      int(text.size()), result.data(), n);
  return result;
}
void component(const std::string &part) {
  if (part.empty() || part.size() > 255 || part == "." || part == ".." ||
      part.back() == '.' || part.back() == ' ' ||
      part.find_first_of("<>:\"/\\|?*") != std::string::npos ||
      std::any_of(part.begin(), part.end(),
                  [](unsigned char c) { return c < 32 || c == 127; }))
    throw std::invalid_argument("Unsafe Windows filename");
  std::string stem = part.substr(0, part.find('.'));
  std::transform(stem.begin(), stem.end(), stem.begin(),
                 [](unsigned char c) { return char(std::toupper(c)); });
  if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" ||
      stem == "CONIN$" || stem == "CONOUT$" ||
      (stem.size() == 4 &&
       (stem.starts_with("COM") || stem.starts_with("LPT")) && stem[3] >= '1' &&
       stem[3] <= '9'))
    throw std::invalid_argument("Reserved Windows filename");
  (void)wide(part);
}
void inspectHandle(HANDLE handle, bool directory) {
  FILE_ATTRIBUTE_TAG_INFO tag{};
  FILE_STANDARD_INFO standard{};
  if (GetFileType(handle) != FILE_TYPE_DISK ||
      !GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag,
                                    sizeof(tag)) ||
      !GetFileInformationByHandleEx(handle, FileStandardInfo, &standard,
                                    sizeof(standard)) ||
      (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
      bool(standard.Directory) != directory)
    throw std::runtime_error(
        "Output is not a regular file/directory or is a reparse point");
}
WinHandle relative(HANDLE parent, const std::wstring &name, ACCESS_MASK access,
                   ULONG disposition, bool directory) {
  using Create = NTSTATUS(NTAPI *)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES,
                                   PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG,
                                   ULONG, ULONG, ULONG, PVOID, ULONG);
  static const auto create = reinterpret_cast<Create>(
      GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateFile"));
  if (!create)
    throw std::runtime_error("Handle-relative file creation is unavailable");
  if (name.empty() || name.size() > 32767)
    throw std::invalid_argument("Invalid native component");
  UNICODE_STRING unicode{};
  unicode.Buffer = const_cast<PWSTR>(name.data());
  unicode.Length = USHORT(name.size() * sizeof(wchar_t));
  unicode.MaximumLength = unicode.Length;
  OBJECT_ATTRIBUTES attributes{};
  attributes.Length = sizeof(attributes);
  attributes.RootDirectory = parent;
  attributes.ObjectName = &unicode;
  attributes.Attributes = 0x40;
  IO_STATUS_BLOCK status{};
  HANDLE handle = INVALID_HANDLE_VALUE;
  const ULONG options =
      0x00200000 | 0x20 |
      (directory
           ? 0x1
           : 0x40); // open reparse point, synchronous, directory/non-directory
  const auto result =
      create(&handle, access | SYNCHRONIZE, &attributes, &status, nullptr,
             FILE_ATTRIBUTE_NORMAL,
             directory ? (FILE_SHARE_READ | FILE_SHARE_WRITE) : FILE_SHARE_READ,
             disposition, options, nullptr, 0);
  if (result < 0)
    throw std::runtime_error("Cannot open/create anchored output (existing "
                             "file, access denied, or unsafe path)");
  WinHandle owned(handle);
  inspectHandle(handle, directory);
  return owned;
}
std::vector<std::string> parts(const std::string &path, bool tree) {
  if (path.empty() || path.size() > 4095 ||
      (!tree && path.find('/') != std::string::npos))
    throw std::invalid_argument("Invalid relative output path");
  std::vector<std::string> result;
  size_t start = 0;
  while (true) {
    const auto end = path.find('/', start);
    auto part =
        path.substr(start, end == std::string::npos ? end : end - start);
    component(part);
    result.push_back(std::move(part));
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  if (result.size() > 33)
    throw std::invalid_argument("Output depth exceeds 32");
  return result;
}
void seek(HANDLE file, uint64_t offset) {
  if (offset > uint64_t(std::numeric_limits<LONGLONG>::max()))
    throw std::runtime_error("Output offset overflow");
  LARGE_INTEGER position{};
  position.QuadPart = LONGLONG(offset);
  if (!SetFilePointerEx(file, position, nullptr, FILE_BEGIN))
    throw std::runtime_error("Output seek failed");
}
void compare(HANDLE file, uint64_t size, const SafeFileExporter::ReadAt &read,
             const std::function<bool()> &cancelled) {
  inspectHandle(file, false);
  FILE_STANDARD_INFO standard{};
  if (!GetFileInformationByHandleEx(file, FileStandardInfo, &standard,
                                    sizeof(standard)) ||
      standard.EndOfFile.QuadPart < 0 ||
      uint64_t(standard.EndOfFile.QuadPart) != size)
    throw std::runtime_error("Saved output size changed");
  for (uint64_t offset = 0; offset < size;) {
    winCancel(cancelled);
    const auto n = size_t(std::min<uint64_t>(65536, size - offset));
    const auto expected = read(offset, n);
    std::vector<uint8_t> actual(n);
    seek(file, offset);
    DWORD got = 0;
    if (!ReadFile(file, actual.data(), DWORD(n), &got, nullptr) || got != n ||
        expected.size() != n || actual != expected)
      throw std::runtime_error("Output verification failed");
    offset += n;
  }
}
WinHandle output(HANDLE parent, const std::string &name, uint64_t size,
                 const SafeFileExporter::ReadAt &read, bool verify,
                 const std::function<bool()> &cancelled) {
  winCancel(cancelled);
  auto file =
      relative(parent, wide(name),
               FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, 2, false);
  try {
    for (uint64_t offset = 0; offset < size;) {
      winCancel(cancelled);
      const auto n = size_t(std::min<uint64_t>(65536, size - offset));
      const auto bytes = read(offset, n);
      if (bytes.size() != n)
        throw std::runtime_error("Incomplete source chunk");
      DWORD written = 0;
      if (!WriteFile(file.value, bytes.data(), DWORD(n), &written, nullptr) ||
          written != n)
        throw std::runtime_error("Output write failed");
      offset += n;
    }
    if (!FlushFileBuffers(file.value))
      throw std::runtime_error("Output flush failed");
    if (verify)
      compare(file.value, size, read, cancelled);
    winCancel(cancelled);
    inspectHandle(file.value, false);
    return file;
  } catch (...) {
    FILE_DISPOSITION_INFO disposition{TRUE};
    SetFileInformationByHandle(file.value, FileDispositionInfo, &disposition,
                               sizeof(disposition));
    throw;
  }
}
} // namespace
struct SafeFileExporter::Impl {
  std::vector<WinHandle> parents;
};
SafeFileExporter::SafeFileExporter(const std::string &directory)
    : impl_(std::make_unique<Impl>()) {
  if (directory.empty() || directory.find(char{0}) != std::string::npos)
    throw std::invalid_argument("Invalid output directory");
  const auto input = disk::nativePath(directory);
  for (const auto &part : input)
    if (part == "..")
      throw std::invalid_argument("Output path contains parent traversal");
  const auto path = std::filesystem::absolute(input).lexically_normal();
  const auto root = path.root_path().wstring();
  if (root.size() != 3 || root[1] != L':' || root[2] != L'\\')
    throw std::invalid_argument("Output must be on a local drive");
  WinHandle drive(CreateFileW(
      root.c_str(),
      FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  if (drive.value == INVALID_HANDLE_VALUE)
    throw std::runtime_error("Cannot open output drive");
  inspectHandle(drive.value, true);
  impl_->parents.push_back(std::move(drive));
  for (const auto &part : path.relative_path()) {
    if (part.empty() || part == ".")
      continue;
    const auto name = disk::utf8Path(part);
    component(name);
    impl_->parents.push_back(relative(
        impl_->parents.back().value, wide(name),
        FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES, 1, true));
  }
}
SafeFileExporter::~SafeFileExporter() = default;
void SafeFileExporter::write(const std::string &name,
                             const std::vector<uint8_t> &data, bool verify,
                             const std::function<bool()> &cancelled) {
  writeStream(
      name, data.size(),
      [&](uint64_t offset, size_t n) {
        return std::vector<uint8_t>(data.begin() + offset,
                                    data.begin() + offset + n);
      },
      verify, false, cancelled);
}
void SafeFileExporter::writeTree(const std::string &name,
                                 const std::vector<uint8_t> &data, bool verify,
                                 const std::function<bool()> &cancelled) {
  writeStream(
      name, data.size(),
      [&](uint64_t offset, size_t n) {
        return std::vector<uint8_t>(data.begin() + offset,
                                    data.begin() + offset + n);
      },
      verify, true, cancelled);
}
void SafeFileExporter::writeStream(const std::string &path, uint64_t size,
                                   const ReadAt &read, bool verify, bool tree,
                                   const std::function<bool()> &cancelled) {
  const auto names = parts(path, tree);
  HANDLE parent = impl_->parents.back().value;
  std::vector<WinHandle> children;
  for (size_t i = 0; i + 1 < names.size(); ++i) {
    winCancel(cancelled);
    children.push_back(relative(
        parent, wide(names[i]),
        FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES, 3, true));
    parent = children.back().value;
  }
  auto file = output(parent, names.back(), size, read, verify, cancelled);
}
void SafeFileExporter::verifyStream(const std::string &path, uint64_t size,
                                    const ReadAt &read, bool tree,
                                    const std::function<bool()> &cancelled) {
  const auto names = parts(path, tree);
  HANDLE parent = impl_->parents.back().value;
  std::vector<WinHandle> children;
  for (size_t i = 0; i + 1 < names.size(); ++i) {
    winCancel(cancelled);
    children.push_back(relative(
        parent, wide(names[i]),
        FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES, 1, true));
    parent = children.back().value;
  }
  auto file = relative(parent, wide(names.back()), FILE_GENERIC_READ, 1, false);
  compare(file.value, size, read, cancelled);
}
void SafeFileExporter::replaceState(const std::string &name,
                                    const std::vector<uint8_t> &data) {
  component(name);
  const auto temporary = ".rs-state-" + std::to_string(std::random_device{}()) +
                         "-" + std::to_string(GetTickCount64());
  const auto read = [&](uint64_t offset, size_t n) {
    return std::vector<uint8_t>(data.begin() + offset,
                                data.begin() + offset + n);
  };
  auto file = output(impl_->parents.back().value, temporary, data.size(), read,
                     true, {});
  try {
    const auto target = wide(name);
    std::vector<uint8_t> buffer(sizeof(FILE_RENAME_INFO) +
                                target.size() * sizeof(wchar_t));
    auto *rename = reinterpret_cast<FILE_RENAME_INFO *>(buffer.data());
    rename->ReplaceIfExists = TRUE;
    rename->RootDirectory = impl_->parents.back().value;
    rename->FileNameLength = DWORD(target.size() * sizeof(wchar_t));
    std::copy(target.begin(), target.end(), rename->FileName);
    if (!SetFileInformationByHandle(file.value, FileRenameInfo, rename,
                                    DWORD(buffer.size())))
      throw std::runtime_error("Cannot atomically commit session checkpoint");
    if (!FlushFileBuffers(file.value))
      throw std::runtime_error("Cannot flush session checkpoint");
  } catch (...) {
    FILE_DISPOSITION_INFO disposition{TRUE};
    SetFileInformationByHandle(file.value, FileDispositionInfo, &disposition,
                               sizeof(disposition));
    throw;
  }
}

#endif
} // namespace recoverysuite::application::service
