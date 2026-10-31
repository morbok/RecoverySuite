#include "Application/Service/device_capture.h"
#include "Application/Service/safe_file_exporter.h"
#include "Disk/Access/disk_image_reader.h"
#include "Disk/Access/path_utf8.h"
#include "Disk/Access/read_only_device.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
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
QTEST_GUILESS_MAIN(TestNativeIo)
#include "test_native_io.moc"
