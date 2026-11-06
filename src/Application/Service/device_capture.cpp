#include "device_capture.h"
#include "Disk/Access/path_utf8.h"
#include "Disk/Access/read_only_device.h"
#include "safe_file_exporter.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <charconv>
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
                               const std::function<bool()> &cancelled,
                               bool resume, uint64_t checkpointInterval) {
  const auto sector = source.getSectorSize();
  const auto total = source.getTotalSectors();
  if (retries > 5 || !source.isOpen() || !source.getDiskInfo().isReadOnly ||
      sector < 512 || sector > 4096 || (sector & (sector - 1)) || !total ||
      total > uint64_t(std::numeric_limits<int64_t>::max()) / sector)
    throw std::runtime_error(
        "Invalid read-only rescue geometry or retry limit");
  const auto path =
      std::filesystem::absolute(disk::nativePath(output)).lexically_normal();
  const auto name = disk::utf8Path(path.filename()),
             report = name + ".rescue.json";
  if (name.empty() || name == "." || name == ".." || name.size() > 240)
    throw std::runtime_error("Choose a valid new rescue image filename");
  std::error_code ec;
  if (std::filesystem::equivalent(
          path, disk::nativePath(source.getDiskInfo().devicePath), ec))
    throw std::runtime_error("Rescue image cannot be the source");
  SafeFileExporter exporter(disk::utf8Path(path.parent_path()));
  const auto check = [&] {
    if (cancelled && cancelled())
      throw std::runtime_error("Rescue cancelled; saved progress is retained");
  };
  check();
  const auto reportPath =
      disk::utf8Path(path.parent_path() / disk::nativePath(report));
  QLockFile ownership(QString::fromStdString(reportPath) + ".lock");
  ownership.setStaleLockTime(0);
  if (!ownership.tryLock(0))
    throw std::runtime_error("This rescue is already in use");
  RescueResult result;
  result.bytes = total * sector;
  result.sectorSize = sector;
  result.report = reportPath;
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

  const auto info = source.getDiskInfo();
  QCryptographicHash stamp(QCryptographicHash::Sha256);
  stamp.addData((QString::fromStdString(info.devicePath) + "\n" +
                 QString::fromStdString(info.serialNumber) + "\n" +
                 QString::number(total) + "\n" + QString::number(sector))
                    .toUtf8());
  // Small deterministic source samples bind restarts without rereading the
  // captured disk prefix. Changed/unreadable samples fail closed on resume.
  for (uint64_t i = 0; i < 16; ++i) {
    const auto at = (total - 1) / 15 * i + ((total - 1) % 15) * i / 15;
    std::vector<std::byte> bytes;
    const bool ok = read(at, 1, bytes);
    stamp.addData(ok ? "readable" : "unreadable");
    if (ok)
      stamp.addData(QByteArrayView(reinterpret_cast<const char *>(bytes.data()),
                                   qsizetype(bytes.size())));
  }
  const auto sourceStamp = stamp.result().toHex().toStdString();
  SafeFileExporter::ResumePoint point{};
  uint64_t copied = 0;
  QJsonObject state;
  const auto number = [](const QJsonValue &value) {
    const auto s = value.toString().toStdString();
    uint64_t n{};
    const auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), n);
    if (s.empty() || error != std::errc{} || end != s.data() + s.size())
      throw std::runtime_error("Invalid rescue checkpoint number");
    return n;
  };
  if (resume) {
    QFileInfo file(QString::fromStdString(reportPath));
    if (!file.isFile() || file.isSymLink() || file.size() > 16 * 1024 * 1024)
      throw std::runtime_error("Invalid rescue report");
    QFile input(file.absoluteFilePath());
    if (!input.open(QIODevice::ReadOnly))
      throw std::runtime_error("Cannot open rescue report");
    const auto bytes = input.read(16 * 1024 * 1024 + 1);
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(bytes, &error);
    if (bytes.size() > 16 * 1024 * 1024 || !input.atEnd() ||
        input.error() != QFileDevice::NoError ||
        error.error != QJsonParseError::NoError || !doc.isObject())
      throw std::runtime_error("Malformed rescue report");
    state = doc.object();
    if (state.value("schema").toInt() != 2 ||
        state.value("image").toString() !=
            QString::fromStdString(disk::utf8Path(path)) ||
        state.value("sourceStamp").toString() !=
            QString::fromStdString(sourceStamp) ||
        number(state.value("sectorSize")) != sector ||
        number(state.value("sectors")) != total)
      throw std::runtime_error("Rescue report belongs to a changed source, "
                               "another image or an unsupported version");
    const auto saved = state.value("checkpoint").toObject();
    point = {number(saved.value("bytes")),
             saved.value("sha256").toString().toStdString(),
             saved.value("identity").toString().toStdString()};
    if (point.bytes > result.bytes || point.bytes % sector ||
        point.identity.empty() || point.identity.size() > 256 ||
        point.sha256.size() != 64)
      throw std::runtime_error("Invalid rescue checkpoint");
    copied = point.bytes;
    if (!state.value("unreadableRanges").isArray() ||
        state.value("unreadableRanges").toArray().size() > 100000)
      throw std::runtime_error("Invalid unreadable range list");
    uint64_t prior = 0;
    for (const auto &value : state.value("unreadableRanges").toArray()) {
      const auto o = value.toObject();
      const auto at = number(o.value("sector")),
                 count = number(o.value("count"));
      if (!count || at < prior || at >= point.bytes / sector ||
          count > point.bytes / sector - at)
        throw std::runtime_error("Invalid committed unreadable range");
      result.unreadable.push_back({at, count});
      result.unreadableSectors += count;
      prior = at + count;
    }
  } else {
    state = QJsonObject{
        {"schema", 2},
        {"status", "incomplete"},
        {"source", QString::fromStdString(info.devicePath)},
        {"image", QString::fromStdString(disk::utf8Path(path))},
        {"sourceStamp", QString::fromStdString(sourceStamp)},
        {"sectorSize", QString::number(sector)},
        {"sectors", QString::number(total)},
        {"warning", "Unreadable sectors are zero-filled, not recovered. Resume "
                    "verifies source samples and the flushed output prefix; it "
                    "does not prove all source bytes unchanged."}};
  }
  const auto saveReport = [&](bool initial) {
    QJsonArray ranges;
    uint64_t gaps = 0;
    for (const auto &r : result.unreadable)
      if (r.sector < point.bytes / sector) {
        const auto count = std::min(r.count, point.bytes / sector - r.sector);
        ranges.append(QJsonObject{{"sector", QString::number(r.sector)},
                                  {"count", QString::number(count)}});
        gaps += count;
      }
    state["unreadableRanges"] = ranges;
    state["unreadableSectors"] = QString::number(gaps);
    const auto bytes = QJsonDocument(state).toJson(QJsonDocument::Compact);
    if (bytes.size() > 16 * 1024 * 1024)
      throw std::runtime_error("Rescue report limit");
    const std::vector<uint8_t> data(bytes.begin(), bytes.end());
    if (initial)
      exporter.write(report, data, true, cancelled);
    else
      exporter.replaceState(report, data);
  };
  if (!resume)
    saveReport(true);
  // Do not rewrite a report if prefix/source validation fails before the
  // checkpoint callback. A bad resume must leave the previous state intact.
  bool begun = false;
  try {
    result.sha256 = exporter.checkpointStream(
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
          copied += n;
          if (progress)
            progress(copied);
          return out;
        },
        resume ? &point : nullptr,
        [&](const SafeFileExporter::ResumePoint &committed) {
          begun = true;
          point = committed;
          state["status"] = "incomplete";
          state["storedBytesVerified"] = false;
          state["checkpoint"] =
              QJsonObject{{"bytes", QString::number(point.bytes)},
                          {"sha256", QString::fromStdString(point.sha256)},
                          {"identity", QString::fromStdString(point.identity)}};
          saveReport(false);
        },
        cancelled, checkpointInterval);
    state["status"] =
        result.unreadableSectors ? "completed-with-gaps" : "completed";
    state["sha256"] = QString::fromStdString(result.sha256);
    state["storedBytesVerified"] = true;
    saveReport(false);
  } catch (...) {
    if (begun) {
      state["status"] = "incomplete";
      state["storedBytesVerified"] = false;
      try {
        saveReport(false);
      } catch (...) {
      }
    }
    throw;
  }
  return result;
}
RescueResult rescueDevice(const std::string &path, const std::string &output,
                          unsigned retries,
                          const std::function<void(uint64_t)> &progress,
                          const std::function<bool()> &cancelled, bool resume) {
  disk::ReadOnlyDevice source;
  source.open(path);
  const auto directory = disk::utf8Path(
      std::filesystem::absolute(disk::nativePath(output)).parent_path());
  source.validateDestination(directory);
  const auto result =
      rescueSourceImage(source, output, retries, progress, cancelled, resume);
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
