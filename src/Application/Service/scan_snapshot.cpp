#include "GUIRecoveryService.h"
#include "safe_file_exporter.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <charconv>
#include <set>
namespace recoverysuite::application::service {
namespace {
constexpr qint64 maxSnapshot = 16 * 1024 * 1024;
uint64_t integer(const QJsonValue &v) {
  const auto s = v.toString().toStdString();
  uint64_t value{};
  const auto [end, error] =
      std::from_chars(s.data(), s.data() + s.size(), value);
  if (s.empty() || error != std::errc{} || end != s.data() + s.size())
    throw std::runtime_error("Invalid saved scan integer");
  return value;
}
std::string text(const QJsonValue &v) {
  if (!v.isString() || v.toString().isEmpty() || v.toString().size() > 4095 ||
      v.toString().contains(QChar(0)))
    throw std::runtime_error("Invalid saved scan text");
  return v.toString().toStdString();
}
QJsonObject document(const std::string &path) {
  QFileInfo info(QString::fromStdString(path));
  if (!info.isFile() || info.isSymLink() || info.size() > maxSnapshot)
    throw std::runtime_error(
        "Choose a regular saved scan no larger than 16 MiB");
  QFile f(info.absoluteFilePath());
  if (!f.open(QIODevice::ReadOnly))
    throw std::runtime_error("Cannot open saved scan");
  const auto bytes = f.read(maxSnapshot + 1);
  if (f.error() != QFileDevice::NoError || bytes.size() > maxSnapshot ||
      !f.atEnd())
    throw std::runtime_error("Saved scan exceeds read limit");
  QJsonParseError error;
  const auto doc = QJsonDocument::fromJson(bytes, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject())
    throw std::runtime_error("Malformed saved scan");
  const auto o = doc.object();
  if (o.value("schema").toInt() != 1 ||
      o.value("engine").toString() != "drive-catalog-v1" ||
      !o.value("physical").isBool())
    throw std::runtime_error("Unsupported saved scan format");
  return o;
}
void safePath(const std::string &path) {
  if (path.empty() || path.front() == '/' ||
      path.find('\\') != std::string::npos ||
      path.find(':') != std::string::npos)
    throw std::runtime_error("Invalid saved candidate path");
  size_t at = 0;
  while (at < path.size()) {
    const auto end = path.find('/', at);
    const auto part =
        path.substr(at, end == std::string::npos ? end : end - at);
    if (part.empty() || part == "." || part == "..")
      throw std::runtime_error("Invalid saved candidate component");
    if (end == std::string::npos)
      break;
    at = end + 1;
    if (at == path.size())
      throw std::runtime_error("Invalid saved candidate path");
  }
}
} // namespace
std::string
GUIRecoveryService::fingerprintSource(disk::IDiskReader &source,
                                      const std::function<bool()> &cancelled) {
  const auto sector = source.getSectorSize();
  const auto total = source.getTotalSectors();
  if (!total || sector < 512 || sector > 4096)
    throw std::runtime_error("Invalid source geometry");
  const auto info = source.getDiskInfo();
  QCryptographicHash hash(QCryptographicHash::Sha256);
  const auto identity = QString::fromStdString(info.devicePath) + "\n" +
                        QString::fromStdString(info.serialNumber) + "\n" +
                        QString::number(sector) + "\n" +
                        QString::number(total) + "\n";
  hash.addData(identity.toUtf8());
  const auto count = std::min<uint64_t>(total, 65536 / sector);
  std::set<uint64_t> starts;
  for (uint64_t i = 0; i < 64; ++i)
    starts.insert((total - count) / 63 * i + ((total - count) % 63) * i / 63);
  for (const auto at : starts) {
    if (cancelled && cancelled())
      throw std::runtime_error("Saved scan validation cancelled");
    std::vector<std::byte> bytes;
    if (!source.readSectors(at, count, bytes) || bytes.size() != count * sector)
      throw std::runtime_error("Cannot fingerprint source for saved scan");
    hash.addData(QByteArrayView(reinterpret_cast<const char *>(bytes.data()),
                                qsizetype(bytes.size())));
  }
  return hash.result().toHex().toStdString();
}
void GUIRecoveryService::saveScan(const std::string &path,
                                  const std::function<bool()> &cancelled) {
  std::unique_lock lock(imageMutex_, std::try_to_lock);
  if (!lock.owns_lock() || !scanValid_ || !sourceReader_ ||
      scanFingerprint_.empty())
    throw std::runtime_error("Start a drive scan before saving it");
  if (imageReader_)
    imageReader_->validateIdentity();
  if (deviceReader_)
    deviceReader_->validateIdentity();
  const QFileInfo file(QString::fromStdString(path));
  if (deviceReader_)
    deviceReader_->validateDestination(file.absolutePath().toStdString());
  if (fingerprintSource(*sourceReader_, cancelled) != scanFingerprint_)
    throw std::runtime_error("Source changed since this scan; scan again");
  QJsonObject obj{
      {"schema", 1},
      {"engine", "drive-catalog-v1"},
      {"source", QString::fromStdString(sourcePath_)},
      {"physical", bool(deviceReader_)},
      {"deepSearch", scanDeepSearch_},
      {"sectorSize", QString::number(sourceReader_->getSectorSize())},
      {"sectors", QString::number(sourceReader_->getTotalSectors())},
      {"fingerprint", QString::fromStdString(scanFingerprint_)}};
  QJsonArray volumes, items;
  for (const auto &v : scanVolumes_)
    volumes.append(
        QJsonObject{{"start", QString::number(v.start)},
                    {"count", QString::number(v.count)},
                    {"filesystem", QString::fromStdString(v.filesystem)}});
  for (const auto &item : savedScan_.recoveredItems)
    items.append(
        QJsonObject{{"name", QString::fromStdString(item.name)},
                    {"size", QString::number(item.sizeBytes)},
                    {"confidence", QString::fromStdString(item.confidence)}});
  QJsonArray issues;
  for (const auto &[key, value] : savedScan_.details)
    if (key.starts_with("Scan issue "))
      issues.append(QString::fromStdString(value).left(4095));
  if (issues.size() > 1000)
    throw std::runtime_error("Saved scan issue limit exceeded");
  obj.insert("issues", issues);
  obj.insert(
      "checkpoint",
      QJsonObject{
          {"volumeSector", QString::number(scanCheckpoint_.volumeSector)},
          {"volumesDone", scanCheckpoint_.volumesDone},
          {"filesVolume", QString::number(scanCheckpoint_.filesVolume)},
          {"filesDone", scanCheckpoint_.filesDone},
          {"signatureOffset", QString::number(scanCheckpoint_.carving.offset)},
          {"signatureItems",
           QString::number(scanCheckpoint_.carving.foundItems)}});
  obj.insert("volumes", volumes);
  obj.insert("items", items);
  const auto bytes = QJsonDocument(obj).toJson(QJsonDocument::Compact);
  if (bytes.size() > maxSnapshot)
    throw std::runtime_error("Saved scan exceeds 16 MiB limit");
  if (cancelled && cancelled())
    throw std::runtime_error("Saving scan cancelled");
  SafeFileExporter out(file.absolutePath().toStdString());
  out.write(file.fileName().toStdString(),
            std::vector<uint8_t>(bytes.begin(), bytes.end()), true, cancelled);
}
models::RecoveryResult
GUIRecoveryService::loadScan(const std::string &path,
                             const std::function<bool()> &cancelled) {
  std::unique_lock lock(imageMutex_, std::try_to_lock);
  if (!lock.owns_lock())
    throw std::runtime_error(
        "Wait for the current operation before opening a saved scan");
  const auto obj = document(path);
  const auto source = text(obj.value("source"));
  const auto sector = integer(obj.value("sectorSize")),
             total = integer(obj.value("sectors"));
  if (sector != 512 && sector != 1024 && sector != 2048 && sector != 4096)
    throw std::runtime_error("Invalid saved sector size");
  const auto fingerprint = text(obj.value("fingerprint"));
  if (fingerprint.size() != 64 ||
      !std::all_of(fingerprint.begin(), fingerprint.end(), [](auto c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
      }))
    throw std::runtime_error("Invalid saved fingerprint");
  std::vector<recovery::FoundVolume> volumes;
  std::map<std::string, uint64_t> catalog;
  models::RecoveryResult result{};
  result.scanOnly = true;
  result.operationType = "disk_scan";
  result.success = true;
  if (!obj.value("volumes").isArray() ||
      obj.value("volumes").toArray().size() > 128 ||
      !obj.value("items").isArray() ||
      obj.value("items").toArray().size() > 10000)
    throw std::runtime_error("Invalid saved scan catalog limits");
  for (const auto &value : obj.value("volumes").toArray()) {
    const auto v = value.toObject();
    const auto start = integer(v.value("start")),
               count = integer(v.value("count"));
    const auto type = text(v.value("filesystem"));
    if (!count || start >= total || count > total - start ||
        (type != "FAT12" && type != "FAT16" && type != "FAT32" &&
         type != "exFAT" && type != "NTFS" && type != "ext4"))
      throw std::runtime_error("Invalid saved volume");
    for (const auto &prior : volumes)
      if (start < prior.start + prior.count && prior.start < start + count)
        throw std::runtime_error("Overlapping saved volumes");
    volumes.push_back({start, count, type});
  }
  recovery::DiskScanCheckpoint checkpoint;
  if (obj.contains("checkpoint")) {
    const auto c = obj.value("checkpoint").toObject();
    if (!c.value("volumesDone").isBool() || !c.value("filesDone").isBool())
      throw std::runtime_error("Invalid saved scan checkpoint");
    checkpoint.volumeSector = integer(c.value("volumeSector"));
    checkpoint.filesVolume = integer(c.value("filesVolume"));
    checkpoint.volumesDone = c.value("volumesDone").toBool();
    checkpoint.filesDone = c.value("filesDone").toBool();
    checkpoint.carving = {integer(c.value("signatureOffset")),
                          integer(c.value("signatureItems"))};
    if (!total || total > uint64_t(INT64_MAX) / sector ||
        checkpoint.volumeSector > total ||
        checkpoint.filesVolume > volumes.size() ||
        checkpoint.carving.offset > total * sector ||
        checkpoint.carving.foundItems > 10000 ||
        (checkpoint.volumesDone && checkpoint.volumeSector != total) ||
        (!checkpoint.volumesDone &&
         (checkpoint.filesVolume || checkpoint.carving.offset ||
          checkpoint.filesDone)) ||
        (checkpoint.filesDone && checkpoint.filesVolume != volumes.size()))
      throw std::runtime_error("Saved scan checkpoint is out of range");
  } else {
    // Original v1 files were completed catalogs without continuation state.
    checkpoint = {
        total, uint64_t(volumes.size()), true, true, {total * sector, 0}};
  }
  for (const auto &value : obj.value("items").toArray()) {
    const auto i = value.toObject();
    const auto name = text(i.value("name")),
               confidence = text(i.value("confidence"));
    const auto size = integer(i.value("size"));
    safePath(name);
    if (size > 2ULL * 1024 * 1024 * 1024 || !catalog.emplace(name, size).second)
      throw std::runtime_error("Invalid or duplicate saved candidate");
    result.recoveredItems.push_back({name, size, confidence});
    result.recoveredBytes += size;
    ++result.recoveredItemsCount;
  }
  if (obj.contains("issues")) {
    if (!obj.value("issues").isArray() ||
        obj.value("issues").toArray().size() > 1000)
      throw std::runtime_error("Invalid saved scan issues");
    size_t index = 1;
    for (const auto &issue : obj.value("issues").toArray())
      result.details["Scan issue " + std::to_string(index++)] = text(issue);
  }
  std::shared_ptr<disk::IDiskReader> reader;
  std::shared_ptr<disk::ReadOnlyDevice> device;
  std::shared_ptr<disk::DiskImageReader> image;
  // Reuse a currently open device: its exclusive handle must not be reopened.
  if (sourceReader_ && source == sourcePath_ &&
      bool(deviceReader_) == obj.value("physical").toBool()) {
    reader = sourceReader_;
    device = deviceReader_;
    image = imageReader_;
  } else if (obj.value("physical").toBool()) {
    device = std::make_shared<disk::ReadOnlyDevice>();
    device->open(source);
    reader = device;
  } else {
    image = std::make_shared<disk::DiskImageReader>(uint32_t(sector));
    image->open(source);
    reader = image;
  }
  if (image)
    image->validateIdentity();
  if (device)
    device->validateIdentity();
  if (reader->getSectorSize() != sector || reader->getTotalSectors() != total ||
      fingerprintSource(*reader, cancelled) != fingerprint)
    throw std::runtime_error(
        "Saved scan belongs to a different or changed source; scan again");
  auto backend = std::make_unique<RecoveryService>(reader);
  result.details["Source"] = source;
  result.details["Filesystem volumes found"] = std::to_string(volumes.size());
  result.details["Saved scan validation"] =
      "Source identity, geometry and up to 4 MiB of samples matched. This is "
      "not a full-content comparison. Selected files are reconstructed again "
      "before preview/export.";
  result.details["Scan"] =
      "Restored candidate catalog; no files have been recovered.";
  sourceReader_ = std::move(reader);
  deviceReader_ = std::move(device);
  imageReader_ = std::move(image);
  sourcePath_ = source;
  scanCheckpoint_ = checkpoint;
  result.details["Scan continuation"] =
      checkpoint.filesDone ? "Completed scan"
                           : "Partial scan: choose Continue interrupted scan";
  scanVolumes_ = std::move(volumes);
  scanCatalog_ = std::move(catalog);
  scanFingerprint_ = fingerprint;
  scanValid_ = true;
  scanDeepSearch_ = obj.value("deepSearch").toBool(true);
  savedScan_ = result;
  recoveryService_ = std::move(backend);
  return result;
}
} // namespace recoverysuite::application::service
