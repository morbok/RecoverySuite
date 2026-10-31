#include "device_capture.h"
#include "Disk/Access/path_utf8.h"
#include "Disk/Access/read_only_device.h"
#include "safe_file_exporter.h"
#include <QCryptographicHash>
#include <limits>
#include <stdexcept>
namespace recoverysuite::application::service {
CaptureResult copySourceImage(disk::IDiskReader &source,
                              const std::string &output,
                              const std::function<void(uint64_t)> &progress,
                              const std::function<bool()> &cancelled) {
  const auto sector = source.getSectorSize();
  const auto sectors = source.getTotalSectors();
  if (!source.isOpen() || !source.getDiskInfo().isReadOnly || sector < 512 ||
      sector > 4096 || (sector & (sector - 1)) || !sectors ||
      sectors > std::numeric_limits<uint64_t>::max() / sector)
    throw std::runtime_error("Invalid read-only capture source");
  const auto path = std::filesystem::absolute(disk::nativePath(output));
  SafeFileExporter exporter(disk::utf8Path(path.parent_path()));
  const auto size = sectors * sector;
  QCryptographicHash hash(QCryptographicHash::Sha256);
  uint64_t hashed = 0;
  exporter.writeStream(
      disk::utf8Path(path.filename()), size,
      [&](uint64_t offset, size_t n) {
        if (cancelled && cancelled())
          throw std::runtime_error("Capture cancelled");
        if (offset % sector || n % sector || n > 65536)
          throw std::runtime_error("Unaligned capture request");
        std::vector<std::byte> bytes;
        if (!source.readSectors(offset / sector, n / sector, bytes) ||
            bytes.size() != n)
          throw std::runtime_error("Physical source read failed; no missing "
                                   "sectors were filled with zeros");
        if (offset == hashed) {
          hash.addData(
              QByteArrayView(reinterpret_cast<const char *>(bytes.data()),
                             qsizetype(bytes.size())));
          hashed += n;
        }
        if (progress)
          progress(offset + n);
        std::vector<uint8_t> result(n);
        for (size_t i = 0; i < n; ++i)
          result[i] = std::to_integer<uint8_t>(bytes[i]);
        return result;
      },
      true, false, cancelled);
  return {size, sector, hash.result().toHex().toStdString()};
}
CaptureResult captureDevice(const std::string &path, const std::string &output,
                            const std::function<void(uint64_t)> &progress,
                            const std::function<bool()> &cancelled) {
  disk::ReadOnlyDevice source;
  source.open(path);
  const auto directory = disk::utf8Path(
      std::filesystem::absolute(disk::nativePath(output)).parent_path());
  source.validateDestination(directory);
  const auto result = copySourceImage(source, output, progress, cancelled);
  source.validateDestination(directory);
  return result;
}
} // namespace recoverysuite::application::service
