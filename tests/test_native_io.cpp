#include "Application/Service/device_capture.h"
#include "Application/Service/safe_file_exporter.h"
#include "Disk/Access/disk_image_reader.h"
#include "Disk/Access/path_utf8.h"
#include "Disk/Access/read_only_device.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#ifdef __linux__
#include "Platform/Linux/Disk/LinuxDiskEnumerator.h"
#endif
using namespace recoverysuite;
class TestNativeIo : public QObject {
  Q_OBJECT
  static void save(const QString &path, const QByteArray &bytes) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size())
      throw std::runtime_error("Fixture write failed");
  }
  static QByteArray read(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
      throw std::runtime_error("Fixture read failed");
    return f.readAll();
  }
  class FaultSource : public disk::IDiskReader {
  public:
    std::map<uint64_t, unsigned> attempts;
    bool fatal = false;
    void open(const std::string &) override {}
    void close() override {}
    bool isOpen() const noexcept override { return true; }
    uint32_t getSectorSize() const noexcept override { return 512; }
    uint64_t getTotalSectors() const noexcept override { return 256; }
    disk::DeviceInformation getDiskInfo() const override {
      disk::DeviceInformation i;
      i.isReadOnly = true;
      i.devicePath = "synthetic-fault-source";
      return i;
    }
    bool writeSectors(uint64_t, uint64_t,
                      const std::vector<std::byte> &) override {
      throw std::logic_error("source write attempted");
    }
    bool readSectors(uint64_t start, uint64_t count,
                     std::vector<std::byte> &bytes) override {
      if (fatal)
        throw disk::DiskException("Source identity changed");
      if (start > 256 || count > 256 - start)
        return false;
      if (count > 1 && start < 6 && start + count > 3)
        throw disk::MediaException("Synthetic batch media error");
      if (count == 1) {
        const auto attempt = ++attempts[start];
        if (start == 4 || start == 5 || (start == 3 && attempt < 2))
          throw disk::BadSectorException("Synthetic unreadable sector");
      }
      bytes.resize(size_t(count * 512));
      for (uint64_t i = 0; i < count; ++i)
        std::fill(bytes.begin() + ptrdiff_t(i * 512),
                  bytes.begin() + ptrdiff_t((i + 1) * 512),
                  std::byte((start + i) % 251));
      return true;
    }
  };

public:
  static void crashCapture(const std::string &path) {
    FaultSource source;
    application::service::rescueSourceImage(
        source, path, 2,
        [](uint64_t bytes) {
          if (bytes > 65536)
            std::_Exit(23);
        },
        {}, false, 65536);
  }
private slots:
  void unicodeImageAndCapture() {
    QTemporaryDir root;
    const auto path = root.filePath(QString::fromUtf8("İmaj 📷.img"));
    QByteArray bytes(131072, char{0});
    for (int i = 0; i < bytes.size(); ++i)
      bytes[i] = char(i % 251);
    save(path, bytes);
    disk::DiskImageReader reader;
    reader.open(path.toStdString());
    std::vector<std::byte> chunk;
    QVERIFY(reader.readSectors(0, 1, chunk));
    QCOMPARE(chunk.size(), size_t{512});
    QVERIFY_EXCEPTION_THROWN(reader.writeSectors(0, 1, chunk),
                             disk::AccessDeniedException);
    const auto output = root.filePath(QString::fromUtf8("Kopya 📷.img"));
    const auto result =
        application::service::copySourceImage(reader, output.toStdString());
    QCOMPARE(result.bytes, uint64_t(bytes.size()));
    QCOMPARE(result.sha256,
             QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
                 .toHex()
                 .toStdString());
    QCOMPARE(read(output), bytes);
    QVERIFY_EXCEPTION_THROWN(
        application::service::copySourceImage(reader, output.toStdString()),
        std::runtime_error);
    QCOMPARE(read(path), bytes);
#ifdef _WIN32
    QVERIFY(!QFile::remove(path));
#else
    QVERIFY(QFile::remove(path));
    QVERIFY_EXCEPTION_THROWN(reader.validateIdentity(), disk::DiskException);
#endif
  }
  void nativeWriterAndState() {
    QTemporaryDir root;
    application::service::SafeFileExporter writer(root.path().toStdString());
    const auto source = [](uint64_t offset, size_t n) {
      return std::vector<uint8_t>(n, uint8_t(offset / 65536));
    };
    const auto name = QString::fromUtf8("Klasör/İç 🧪.bin").toStdString();
    writer.writeStream(name, 150000, source, true, true);
    writer.verifyStream(name, 150000, source, true);
    QCOMPARE(read(root.filePath(QString::fromStdString(name))).size(), 150000);
    QVERIFY_EXCEPTION_THROWN(
        writer.writeStream(name, 150000, source, true, true),
        std::runtime_error);
    writer.replaceState("session.json", {'o', 'l', 'd'});
    writer.replaceState("session.json", {'n', 'e', 'w'});
    QCOMPARE(read(root.filePath("session.json")), QByteArray("new"));
    for (const std::string unsafe :
         {"../outside", "a/../b", "a//b", "a:b", "a\\b", "/absolute"})
      QVERIFY_EXCEPTION_THROWN(
          writer.writeStream(unsafe, 1, source, true, true),
          std::invalid_argument);
#ifdef _WIN32
    for (const std::string unsafe : {"CON", "NUL.txt", "COM1", "LPT9.log",
                                     "trailing.", "trailing ", "a?b", "a|b"})
      QVERIFY_EXCEPTION_THROWN(
          writer.writeStream(unsafe, 1, source, true, true),
          std::invalid_argument);
#endif
    save(root.filePath(QString::fromStdString(name)), "changed");
    QVERIFY_EXCEPTION_THROWN(writer.verifyStream(name, 150000, source, true),
                             std::runtime_error);
  }
  void rejectsOversizedOutput() {
    QTemporaryDir root;
    application::service::SafeFileExporter writer(root.path().toStdString());
    bool requested = false;
    const auto source = [&](uint64_t, size_t n) {
      requested = true;
      return std::vector<uint8_t>(n);
    };
    QVERIFY_EXCEPTION_THROWN(
        writer.writeStream("oversized.bin",
                           std::numeric_limits<uint64_t>::max(), source, true,
                           false),
        std::runtime_error);
    QVERIFY(!requested);
    QVERIFY(!QFileInfo::exists(root.filePath("oversized.bin")));
  }
  void checkpointUnicodeAndFailedReplacement() {
    QTemporaryDir root;
    application::service::SafeFileExporter writer(root.path().toStdString());
    const auto name = QString::fromUtf8("Oturum 🧪.json");
    writer.replaceState(name.toStdString(), {'o', 'l', 'd'});
    writer.replaceState(name.toStdString(), {'n', 'e', 'w'});
    QCOMPARE(read(root.filePath(name)), QByteArray("new"));
    QVERIFY(QDir().mkdir(root.filePath("occupied")));
    save(root.filePath("occupied/keep.bin"), "keep");
    QVERIFY_EXCEPTION_THROWN(writer.replaceState("occupied", {'x'}),
                             std::runtime_error);
    QCOMPARE(read(root.filePath("occupied/keep.bin")), QByteArray("keep"));
    QCOMPARE(read(root.filePath(name)), QByteArray("new"));
    QCOMPARE(QDir(root.path())
                 .entryList({".rs-state-*"}, QDir::Files | QDir::Hidden)
                 .size(),
             0);
  }
  void rejectsLinks() {
    QTemporaryDir root;
    QVERIFY(QDir().mkdir(root.filePath("real")));
    std::error_code ec;
    std::filesystem::create_directory_symlink(
        disk::nativePath(root.filePath("real").toStdString()),
        disk::nativePath(root.filePath("link").toStdString()), ec);
    if (ec)
      QSKIP("Host cannot create directory symlinks");
    application::service::SafeFileExporter writer(root.path().toStdString());
    QVERIFY_EXCEPTION_THROWN(writer.writeTree("link/escape", {1, 2, 3}, true),
                             std::runtime_error);
    QVERIFY(!QFileInfo::exists(root.filePath("real/escape")));
    QVERIFY_EXCEPTION_THROWN(application::service::SafeFileExporter(
                                 root.filePath("link").toStdString()),
                             std::runtime_error);
  }
  void rejectsNonDevicesAndCancellation() {
    QTemporaryDir root;
    const auto path = root.filePath("source.img");
    save(path, QByteArray(131072, 'X'));
    disk::ReadOnlyDevice device;
    QVERIFY_EXCEPTION_THROWN(device.open(path.toStdString()),
                             disk::DiskException);
    QVERIFY(!device.isOpen());
    QVERIFY_EXCEPTION_THROWN(device.writeSectors(0, 1, {}),
                             disk::AccessDeniedException);
    disk::DiskImageReader reader;
    reader.open(path.toStdString());
    bool cancel = false;
    const auto output = root.filePath("cancel.img");
    QVERIFY_EXCEPTION_THROWN(application::service::copySourceImage(
                                 reader, output.toStdString(),
                                 [&](uint64_t) { cancel = true; },
                                 [&] { return cancel; }),
                             std::runtime_error);
    QVERIFY(!QFileInfo::exists(output));
  }
  void rescueRetriesAndReportsGapsWithoutRereadingMedia() {
    QTemporaryDir root;
    FaultSource source;
    const auto path = root.filePath("rescue.img");
    const auto result =
        application::service::rescueSourceImage(source, path.toStdString());
    QCOMPARE(result.bytes, uint64_t(131072));
    QCOMPARE(result.unreadableSectors, uint64_t(2));
    QCOMPARE(source.attempts[3], 2u);
    QCOMPARE(source.attempts[4], 3u);
    QCOMPARE(source.attempts[5], 3u);
    QCOMPARE(result.unreadable.size(), size_t(1));
    QCOMPARE(result.unreadable[0].sector, uint64_t(4));
    QCOMPARE(result.unreadable[0].count, uint64_t(2));
    QByteArray expected(131072, char{0});
    for (int sector = 0; sector < 256; ++sector)
      if (sector != 4 && sector != 5)
        expected.replace(sector * 512, 512,
                         QByteArray(512, char(sector % 251)));
    QCOMPARE(read(path), expected);
    QCOMPARE(result.sha256,
             QCryptographicHash::hash(expected, QCryptographicHash::Sha256)
                 .toHex()
                 .toStdString());
    const auto report =
        QJsonDocument::fromJson(read(QString::fromStdString(result.report)))
            .object();
    QCOMPARE(report["status"].toString(), QString("completed-with-gaps"));
    QVERIFY(report["storedBytesVerified"].toBool());
    QCOMPARE(report["unreadableRanges"].toArray().size(), 1);
    QVERIFY_EXCEPTION_THROWN(
        application::service::rescueSourceImage(source, path.toStdString()),
        std::runtime_error);
    QCOMPARE(read(path), expected);
  }
  void rescueStopsOnIdentityErrorsAndCancellation() {
    QTemporaryDir root;
    FaultSource source;
    source.fatal = true;
    const auto path = root.filePath("fatal.img");
    QVERIFY_EXCEPTION_THROWN(
        application::service::rescueSourceImage(source, path.toStdString()),
        disk::DiskException);
    QVERIFY(!QFile::exists(path));
    QVERIFY(!QFile::exists(path + ".rescue.json"));
    source.fatal = false;
    bool cancelled = false;
    const auto cancel = root.filePath("cancel.img");
    QVERIFY_EXCEPTION_THROWN(application::service::rescueSourceImage(
                                 source, cancel.toStdString(), 2,
                                 [&](uint64_t) { cancelled = true; },
                                 [&] { return cancelled; }),
                             std::runtime_error);
    QVERIFY(QFile::exists(cancel));
    QVERIFY(QFile::exists(cancel + ".rescue.json"));
  }
  void rescueResumesCommittedPrefixAndRejectsTampering() {
    QTemporaryDir root;
    FaultSource source;
    const auto path = root.filePath("resume.img");
    bool stop = false;
    QVERIFY_EXCEPTION_THROWN(application::service::rescueSourceImage(
                                 source, path.toStdString(), 2,
                                 [&](uint64_t bytes) {
                                   if (bytes > 65536)
                                     stop = true;
                                 },
                                 [&] { return stop; }, false, 65536),
                             std::runtime_error);
    QCOMPARE(read(path).size(), 65536);
    const auto originalReport = read(path + ".rescue.json");
    const auto checkpoint = QJsonDocument::fromJson(originalReport)
                                .object()["checkpoint"]
                                .toObject();
    QCOMPARE(checkpoint["bytes"].toString(), QString("65536"));
    const auto original = read(path);
    auto damaged = original;
    damaged[0] = char(uint8_t(damaged[0]) ^ 1);
    save(path, damaged);
    QVERIFY_EXCEPTION_THROWN(
        application::service::rescueSourceImage(source, path.toStdString(), 2,
                                                {}, {}, true, 65536),
        std::runtime_error);
    QCOMPARE(read(path), damaged);
    QCOMPARE(read(path + ".rescue.json"), originalReport);
    save(path, original);
    const auto before = source.attempts[3];
    const auto result = application::service::rescueSourceImage(
        source, path.toStdString(), 2, {}, {}, true, 65536);
    QCOMPARE(source.attempts[3], before);
    QCOMPARE(result.unreadableSectors, uint64_t(2));
    QCOMPARE(read(path).size(), 131072);
    QCOMPARE(result.sha256,
             QCryptographicHash::hash(read(path), QCryptographicHash::Sha256)
                 .toHex()
                 .toStdString());
    QCOMPARE(QJsonDocument::fromJson(read(path + ".rescue.json"))
                 .object()["status"]
                 .toString(),
             QString("completed-with-gaps"));
  }
  void checkpointTruncatesUncommittedTailAndBindsFileIdentity() {
    QTemporaryDir root;
    application::service::SafeFileExporter output(root.path().toStdString());
    application::service::SafeFileExporter::ResumePoint point;
    bool stop = false;
    const auto source = [&](uint64_t offset, size_t n) {
      if (offset >= 65536)
        stop = true;
      return std::vector<uint8_t>(n, 7);
    };
    QVERIFY_EXCEPTION_THROWN(output.checkpointStream(
                                 "partial.img", 131072, source, nullptr,
                                 [&](const auto &p) { point = p; },
                                 [&] { return stop; }, 65536),
                             std::runtime_error);
    QCOMPARE(point.bytes, uint64_t(65536));
    // Simulate a process stopping after a partial, uncommitted final write.
    QFile tail(root.filePath("partial.img"));
    QVERIFY(tail.open(QIODevice::Append));
    QCOMPARE(tail.write("junk"), qint64(4));
    tail.close();
    uint64_t first = 0;
    const auto hash = output.checkpointStream(
        "partial.img", 131072,
        [&](uint64_t offset, size_t n) {
          if (!first)
            first = offset;
          return std::vector<uint8_t>(n, 9);
        },
        &point, [&](const auto &) {}, {}, 65536);
    QCOMPARE(first, uint64_t(65536));
    QCOMPARE(read(root.filePath("partial.img")),
             QByteArray(65536, char(7)) + QByteArray(65536, char(9)));
    QCOMPARE(hash, QCryptographicHash::hash(read(root.filePath("partial.img")),
                                            QCryptographicHash::Sha256)
                       .toHex()
                       .toStdString());
    const auto before = read(root.filePath("partial.img"));
    auto forged = point;
    forged.identity = "not-this-file";
    QVERIFY_EXCEPTION_THROWN(output.checkpointStream(
                                 "partial.img", 131072, source, &forged,
                                 [](const auto &) {}, {}, 65536),
                             std::runtime_error);
    QCOMPARE(read(root.filePath("partial.img")), before);
  }
  void rescueRestartsAfterProcessCrash() {
    QTemporaryDir root;
    const auto path = root.filePath("crash.img");
    QProcess child;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("RS_CRASH_RESCUE", path);
    child.setProcessEnvironment(environment);
    child.start(QCoreApplication::applicationFilePath(), {});
    QVERIFY(child.waitForStarted(10000));
    QVERIFY(child.waitForFinished(10000));
    QCOMPARE(child.exitCode(), 23);
    const auto obj =
        QJsonDocument::fromJson(read(path + ".rescue.json")).object();
    QCOMPARE(obj["checkpoint"].toObject()["bytes"].toString(),
             QString("65536"));
    FaultSource source;
    const auto result = application::service::rescueSourceImage(
        source, path.toStdString(), 2, {}, {}, true, 65536);
    QCOMPARE(result.bytes, uint64_t(131072));
    QCOMPARE(result.unreadableSectors, uint64_t(2));
    QCOMPARE(source.attempts[3], 0u);
    QCOMPARE(read(path).size(), 131072);
  }
  void storedDigestVerificationNeverRequestsSourceTwice() {
    QTemporaryDir root;
    application::service::SafeFileExporter writer(root.path().toStdString());
    std::set<uint64_t> requested;
    writer.writeStream(
        "once.bin", 131072,
        [&](uint64_t at, size_t n) {
          if (!requested.insert(at).second)
            throw std::runtime_error("Source was reread");
          return std::vector<uint8_t>(n, 7);
        },
        true, false, {}, true);
    QCOMPARE(requested.size(), size_t(2));
    QCOMPARE(read(root.filePath("once.bin")), QByteArray(131072, char(7)));
  }
  void observedSysfsInventory() {
#ifdef __linux__
    QTemporaryDir root;
    const auto sys = root.filePath("sys");
    QVERIFY(QDir().mkpath(sys + "/disk0/queue"));
    QVERIFY(QDir().mkpath(sys + "/disk0/device"));
    QVERIFY(QDir().mkpath(sys + "/part0"));
    save(sys + "/disk0/dev", "8:0\n");
    save(sys + "/disk0/size", "64\n");
    save(sys + "/disk0/queue/logical_block_size", "512\n");
    save(sys + "/disk0/queue/physical_block_size", "4096\n");
    save(sys + "/disk0/device/model", "Observed Disk  \n");
    save(sys + "/disk0/ro", "1\n");
    save(sys + "/part0/partition", "1\n");
    platform::linux::LinuxDiskEnumerator inventory(
        disk::nativePath(sys.toStdString()),
        disk::nativePath(root.filePath("dev").toStdString()));
    const auto list = inventory.enumerateDisks();
    QCOMPARE(list.size(), size_t{1});
    QCOMPARE(list.front().modelNumber, std::string("Observed Disk"));
    QCOMPARE(list.front().totalSizeBytes, uint64_t{32768});
    QCOMPARE(list.front().physicalBytesPerSector, uint32_t{4096});
    QVERIFY(list.front().isReadOnly);
    QCOMPARE(inventory.getDiskInfo(uint64_t{8} << 32).totalSectors,
             uint64_t{64});
    QVERIFY_EXCEPTION_THROWN(inventory.getDiskInfo(0), disk::DiskException);
#endif
  }
};
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  const auto crash = qEnvironmentVariable("RS_CRASH_RESCUE");
  if (!crash.isEmpty()) {
    TestNativeIo::crashCapture(crash.toStdString());
    return 24;
  }
  TestNativeIo test;
  return QTest::qExec(&test, argc, argv);
}
#include "test_native_io.moc"
