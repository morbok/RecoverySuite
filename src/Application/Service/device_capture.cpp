#include "device_capture.h"
#include "Disk/Access/path_utf8.h"
#include "Disk/Access/read_only_device.h"
#include "safe_file_exporter.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
RescueResult rescueSourceImage(disk::IDiskReader &source,
                               const std::string &output, unsigned retries,
                               const std::function<void(uint64_t)> &progress,
                               const std::function<bool()> &cancelled) {
  const auto sector = source.getSectorSize();
  const auto total = source.getTotalSectors();
  if (retries > 5 || !source.isOpen() || !source.getDiskInfo().isReadOnly ||
      sector < 512 || sector > 4096 || (sector & (sector - 1)) || !total ||
      total > uint64_t(std::numeric_limits<int64_t>::max()) / sector)
    throw std::runtime_error(
        "Invalid read-only rescue geometry or retry limit");
  const auto path = std::filesystem::absolute(disk::nativePath(output));
  const auto name = disk::utf8Path(path.filename()),
             report = name + ".rescue.json";
  SafeFileExporter exporter(disk::utf8Path(path.parent_path()));
  const auto check = [&] {
    if (cancelled && cancelled())
      throw std::runtime_error("Rescue cancelled");
  };
  check();
  // Reserve the mandatory report first; a report collision must prevent
  // imaging.
  QJsonObject state{
      {"schema", 1},
      {"status", "incomplete"},
      {"source", QString::fromStdString(source.getDiskInfo().devicePath)},
      {"sectorSize", QString::number(sector)},
      {"sectors", QString::number(total)},
      {"warning",
       "Unreadable sectors are zero-filled, not recovered. Incomplete imaging "
       "must not be treated as a complete rescue."}};
  const auto saveReport = [&](bool initial) {
    const auto bytes = QJsonDocument(state).toJson(QJsonDocument::Compact);
    const std::vector<uint8_t> data(bytes.begin(), bytes.end());
    if (initial)
      exporter.write(report, data, true, cancelled);
    else
      exporter.replaceState(report, data);
  };
  saveReport(true);
  RescueResult result;
  result.bytes = total * sector;
  result.sectorSize = sector;
  result.report = disk::utf8Path(path.parent_path() / disk::nativePath(report));
  QCryptographicHash hash(QCryptographicHash::Sha256);
  uint64_t copied = 0;
  const auto read = [&](uint64_t at, uint64_t count,
                        std::vector<std::byte> &bytes) {
    check();
    bytes.clear();
    try {
      return source.readSectors(at, count, bytes) &&
             bytes.size() == count * sector;
    } catch (const disk::MediaException &) {
      return false;
    } catch (const disk::BadSectorException &) {
      return false;
    } catch (const disk::IoTimeoutException &) {
      return false;
    }
  };
  try {
    exporter.writeStream(
        name, result.bytes,
        [&](uint64_t offset, size_t n) {
          check();
          if (offset != copied || offset % sector || n % sector || n > 65536)
            throw std::runtime_error("Unexpected rescue stream order");
          std::vector<uint8_t> out(n, 0);
          std::vector<std::byte> bytes;
          if (read(offset / sector, n / sector, bytes)) {
            for (size_t i = 0; i < n; ++i)
              out[i] = std::to_integer<uint8_t>(bytes[i]);
          } else {
            for (size_t at = 0; at < n; at += sector) {
              const auto lba = (offset + at) / sector;
              bool ok = false;
              for (unsigned attempt = 0; attempt <= retries; ++attempt)
                if (read(lba, 1, bytes)) {
                  ok = true;
                  break;
                }
              if (ok)
                for (size_t i = 0; i < sector; ++i)
                  out[at + i] = std::to_integer<uint8_t>(bytes[i]);
              else {
                ++result.unreadableSectors;
                if (!result.unreadable.empty() &&
                    result.unreadable.back().sector +
                            result.unreadable.back().count ==
                        lba)
                  ++result.unreadable.back().count;
                else {
                  if (result.unreadable.size() >= 100000)
                    throw std::runtime_error(
                        "Rescue unreadable-range limit (100,000)");
                  result.unreadable.push_back({lba, 1});
                }
              }
            }
          }
          hash.addData(
              QByteArrayView(reinterpret_cast<const char *>(out.data()),
                             qsizetype(out.size())));
          copied += n;
          if (progress)
            progress(copied);
          return out;
        },
        true, false, cancelled, true);
    result.sha256 = hash.result().toHex().toStdString();
    state["status"] =
        result.unreadableSectors ? "completed-with-gaps" : "completed";
    state["sha256"] = QString::fromStdString(result.sha256);
    state["storedBytesVerified"] = true;
  } catch (...) {
    state["status"] = "incomplete";
    state["sourceBytesRead"] = QString::number(copied);
    QJsonArray ranges;
    for (const auto &r : result.unreadable)
      ranges.append(QJsonObject{{"sector", QString::number(r.sector)},
                                {"count", QString::number(r.count)}});
    state["unreadableRanges"] = ranges;
    state["unreadableSectors"] = QString::number(result.unreadableSectors);
    try {
      saveReport(false);
    } catch (...) {
    } // Preserve the original capture error.
    throw;
  }
  QJsonArray ranges;
  for (const auto &r : result.unreadable)
    ranges.append(QJsonObject{{"sector", QString::number(r.sector)},
                              {"count", QString::number(r.count)}});
  state["unreadableRanges"] = ranges;
  state["unreadableSectors"] = QString::number(result.unreadableSectors);
  saveReport(false);
  return result;
}
RescueResult rescueDevice(const std::string &path, const std::string &output,
                          unsigned retries,
                          const std::function<void(uint64_t)> &progress,
                          const std::function<bool()> &cancelled) {
  disk::ReadOnlyDevice source;
  source.open(path);
  const auto directory = disk::utf8Path(
      std::filesystem::absolute(disk::nativePath(output)).parent_path());
  source.validateDestination(directory);
  const auto result =
      rescueSourceImage(source, output, retries, progress, cancelled);
  source.validateDestination(directory);
  return result;
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
