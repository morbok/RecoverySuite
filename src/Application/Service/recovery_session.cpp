#include "recovery_session.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <charconv>
#include <stdexcept>
namespace recoverysuite::application::service {
namespace {
[[noreturn]] void fail(const char *text) { throw std::runtime_error(text); }
uint64_t number(const QJsonValue &value) {
  const auto text = value.toString().toStdString();
  uint64_t n{};
  const auto [end, ec] =
      std::from_chars(text.data(), text.data() + text.size(), n);
  if (ec != std::errc{} || end != text.data() + text.size() || text.empty())
    fail("Invalid session integer");
  return n;
}
std::string string(const QJsonObject &object, const char *key) {
  const auto value = object.value(key);
  if (!value.isString() || value.toString().isEmpty() ||
      value.toString().size() > 4095 || value.toString().contains(QChar(0)))
    fail("Invalid session string");
  return value.toString().toStdString();
}
QJsonObject document(const std::string &path) {
  QFileInfo info(QString::fromStdString(path));
  if (info.isSymLink() || !info.isFile() || info.size() > 16 * 1024 * 1024)
    fail("Session must be a regular file below 16 MiB");
  QFile file(info.absoluteFilePath());
  if (!file.open(QIODevice::ReadOnly))
    fail("Cannot read session");
  QJsonParseError error{};
  const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject())
    fail("Malformed session JSON");
  const auto obj = doc.object();
  if (obj.value("schema").toInt() != 1 ||
      obj.value("engine").toString() != "recovery-v2")
    fail("Unsupported session schema or recovery engine");
  return obj;
}
SessionDefinition definition(const QJsonObject &obj, const std::string &path) {
  SessionDefinition d;
  d.operation.operationType = string(obj, "method");
  d.operation.targetDevicePath = string(obj, "source");
  d.operation.outputPath = string(obj, "output");
  d.operation.startSector = number(obj.value("start"));
  d.operation.numSectors = number(obj.value("count"));
  const auto sector = number(obj.value("sectorSize"));
  if ((sector != 512 && sector != 1024 && sector != 2048 && sector != 4096) ||
      !d.operation.numSectors || !obj.value("verify").isBool())
    fail("Invalid session geometry/options");
  d.sectorSize = uint32_t(sector);
  d.operation.verifyAfterRecovery = obj.value("verify").toBool();
  d.sourceHash = string(obj, "sourceSha256");
  if (d.sourceHash.size() != 64 ||
      !std::all_of(d.sourceHash.begin(), d.sourceHash.end(),
                   [](unsigned char c) {
                     return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
                   }))
    fail("Invalid session fingerprint");
  d.status = string(obj, "status");
  if (d.status != "running" && d.status != "partial" &&
      d.status != "cancelled" && d.status != "completed")
    fail("Invalid session status");
  d.operation.sessionPath = path;
  d.operation.resumeSession = true;
  return d;
}
} // namespace
SessionDefinition RecoverySession::inspect(const std::string &path) {
  return definition(document(path), path);
}
RecoverySession::RecoverySession(
    const models::RecoveryOperation &operation, disk::IDiskReader &source,
    SafeFileExporter &output, const std::function<bool()> &cancelled,
    const std::function<void(uint64_t)> &fingerprintProgress)
    : operation_(operation), sectorSize_(source.getSectorSize()),
      output_(output), cancelled_(cancelled) {
  QFileInfo file(QString::fromStdString(operation.sessionPath));
  if (file.isSymLink() || file.fileName().isEmpty())
    fail("Unsafe session path");
  const auto sessionPath = file.absoluteFilePath();
  const auto sourcePath =
      QFileInfo(QString::fromStdString(operation.targetDevicePath))
          .canonicalFilePath();
  if (sessionPath == sourcePath || (!file.canonicalFilePath().isEmpty() &&
                                    file.canonicalFilePath() == sourcePath))
    fail("Session cannot replace the source image");
  state_ =
      std::make_unique<SafeFileExporter>(file.absolutePath().toStdString());
  filename_ = file.fileName().toStdString();
  lock_ = std::make_unique<QLockFile>(sessionPath + ".lock");
  lock_->setStaleLockTime(0);
  if (!lock_->tryLock(0))
    fail("This session is already in use");
  if (operation.resumeSession) {
    const auto obj = document(operation.sessionPath);
    const auto prior = definition(obj, operation.sessionPath);
    const auto &op = prior.operation;
    if (op.operationType != operation.operationType ||
        op.targetDevicePath != operation.targetDevicePath ||
        QFileInfo(QString::fromStdString(op.outputPath)).canonicalFilePath() !=
            QFileInfo(QString::fromStdString(operation.outputPath))
                .canonicalFilePath() ||
        op.startSector != operation.startSector ||
        op.numSectors != operation.numSectors ||
        prior.sectorSize != sectorSize_ ||
        op.verifyAfterRecovery != operation.verifyAfterRecovery)
      fail("Session configuration differs from the saved operation");
    sourceHash_ = prior.sourceHash;
    const auto entries = obj.value("entries");
    if (!entries.isArray() || entries.toArray().size() > 10000)
      fail("Invalid session output list");
    for (const auto &item : entries.toArray()) {
      if (!item.isObject())
        fail("Invalid session output entry");
      const auto o = item.toObject();
      const auto name = string(o, "name"), hash = string(o, "sha256");
      if (hash.size() != 64 || !o.value("complete").isBool() ||
          !entries_
               .emplace(name, Entry{number(o.value("size")), hash,
                                    o.value("complete").toBool()})
               .second)
        fail("Duplicate or invalid session output entry");
    }
  } else if (file.exists())
    fail("Session already exists; use Resume session");
  QCryptographicHash hash(QCryptographicHash::Sha256);
  const auto total = source.getTotalSectors(),
             batch = uint64_t{65536} / sectorSize_;
  for (uint64_t offset = 0; offset < total;) {
    if (cancelled_ && cancelled_())
      fail("Source fingerprint cancelled");
    const auto count = std::min(batch, total - offset);
    std::vector<std::byte> bytes;
    if (!source.readSectors(offset, count, bytes) ||
        bytes.size() != count * sectorSize_)
      fail("Source fingerprint read failed");
    hash.addData(QByteArrayView(reinterpret_cast<const char *>(bytes.data()),
                                qsizetype(bytes.size())));
    offset += count;
    if (fingerprintProgress)
      fingerprintProgress(offset * sectorSize_);
  }
  const auto current = hash.result().toHex().toStdString();
  if (!sourceHash_.empty() && sourceHash_ != current)
    fail("Source content changed; session cannot resume");
  sourceHash_ = current;
  save();
}
void RecoverySession::save() {
  QJsonObject obj{
      {"schema", 1},
      {"engine", "recovery-v2"},
      {"method", QString::fromStdString(operation_.operationType)},
      {"source", QString::fromStdString(operation_.targetDevicePath)},
      {"output", QFileInfo(QString::fromStdString(operation_.outputPath))
                     .canonicalFilePath()},
      {"start", QString::number(operation_.startSector)},
      {"count", QString::number(operation_.numSectors)},
      {"sectorSize", QString::number(sectorSize_)},
      {"verify", operation_.verifyAfterRecovery},
      {"sourceSha256", QString::fromStdString(sourceHash_)},
      {"status", QString::fromStdString(status_)}};
  QJsonArray entries;
  for (const auto &[name, e] : entries_)
    entries.append(QJsonObject{{"name", QString::fromStdString(name)},
                               {"size", QString::number(e.size)},
                               {"sha256", QString::fromStdString(e.hash)},
                               {"complete", e.complete}});
  obj.insert("entries", entries);
  const auto bytes = QJsonDocument(obj).toJson(QJsonDocument::Compact);
  state_->replaceState(filename_,
                       std::vector<uint8_t>(bytes.begin(), bytes.end()));
}
bool RecoverySession::exportItem(const std::string &name, uint64_t size,
                                 const SafeFileExporter::ReadAt &read,
                                 bool tree, bool verify) {
  QCryptographicHash hash(QCryptographicHash::Sha256);
  for (uint64_t offset = 0; offset < size;) {
    if (cancelled_ && cancelled_())
      fail("Session output cancelled");
    const auto n =
        static_cast<size_t>(std::min<uint64_t>(65536, size - offset));
    const auto bytes = read(offset, n);
    if (bytes.size() != n)
      fail("Incomplete session source chunk");
    hash.addData(QByteArrayView(reinterpret_cast<const char *>(bytes.data()),
                                qsizetype(n)));
    offset += n;
  }
  seen_.insert(name);
  const auto digest = hash.result().toHex().toStdString();
  auto found = entries_.find(name);
  if (found != entries_.end()) {
    if (found->second.size != size || found->second.hash != digest)
      fail("Session output no longer matches discovered source bytes");
    if (QFileInfo::exists(QDir(QString::fromStdString(operation_.outputPath))
                              .filePath(QString::fromStdString(name)))) {
      output_.verifyStream(name, size, read, tree, cancelled_);
      found->second.complete = true;
      save();
      return true;
    }
  } else {
    if (entries_.size() >= 10000)
      fail("Session output limit reached");
    entries_.emplace(name, Entry{size, digest, false});
    save();
  }
  output_.writeStream(name, size, read, verify, tree, cancelled_);
  entries_.at(name).complete = true;
  save();
  return false;
}
void RecoverySession::finish(const std::string &status) {
  if (status == "completed" && seen_.size() != entries_.size()) {
    status_ = "partial";
    save();
    fail("Some saved outputs were not rediscovered; session remains partial");
  }
  status_ = status;
  save();
}
} // namespace recoverysuite::application::service
