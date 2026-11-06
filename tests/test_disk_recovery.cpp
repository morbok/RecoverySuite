#include "Application/Service/GUIRecoveryService.h"
#include "GUI/core/DeviceAccess.h"
#include "GUI/core/MainWindow.h"
#include "GUI/widgets/RecoveryResultsWidget.h"
#include "GUI/widgets/SourceDiscoveryWidget.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
using namespace recoverysuite;
using namespace recoverysuite::application::service;
class TestDiskRecovery : public QObject {
  Q_OBJECT
  static void put(QByteArray &b, int offset, uint64_t value, int n = 2) {
    for (int i = 0; i < n; ++i)
      b[offset + i] = char(value >> (8 * i));
  }
  static QByteArray disk(bool damagedBoot = false) {
    QByteArray volume(64 * 512, char{0});
    put(volume, 11, 512);
    volume[13] = 1;
    put(volume, 14, 1);
    volume[16] = 2;
    put(volume, 17, 16);
    put(volume, 19, 64);
    volume[21] = char(0xf8);
    put(volume, 22, 1);
    put(volume, 510, 0xaa55);
    for (int offset : {512, 1024})
      volume.replace(offset, 6, QByteArray::fromHex("f8ffffff0f00"));
    const int root = 3 * 512;
    volume.replace(root, 11, "HELLO   TXT");
    volume[root + 11] = 0x20;
    put(volume, root + 26, 2);
    put(volume, root + 28, 5, 4);
    volume.replace(4 * 512, 5, "hello");
    volume.replace(root + 32, 11, "GONE    TXT");
    volume[root + 32] = char(0xe5);
    volume[root + 43] = 0x20;
    put(volume, root + 58, 3);
    put(volume, root + 60, 5, 4);
    volume.replace(5 * 512, 5, "later");
    volume.replace(7 * 512, 10, QByteArray::fromHex("ffd8ff68656c6c6fffd9"));
    if (damagedBoot)
      volume[13] = 0;
    // Deliberately no partition entry: the former volume begins at sector 32.
    return QByteArray(32 * 512, char{0}) + volume +
           QByteArray(16 * 512, char{0});
  }
  // Single-FAT exFAT volume without a partition table. Includes contiguous,
  // fragmented, deleted and nested Unicode-name files plus uninitialized tail.
  static QByteArray exfatDisk() {
    QByteArray v(128 * 512, char{0});
    v.replace(3, 8, "EXFAT   ");
    put(v, 72, 128, 8);
    put(v, 80, 24, 4);
    put(v, 84, 1, 4);
    put(v, 88, 25, 4);
    put(v, 92, 103, 4);
    put(v, 96, 2, 4);
    put(v, 104, 0x100);
    v[108] = 9;
    v[109] = 0;
    v[110] = 1;
    put(v, 510, 0xaa55);
    for (int c : {2, 3, 4, 6, 8, 9})
      put(v, 24 * 512 + c * 4, 0xffffffff, 4);
    put(v, 24 * 512 + 5 * 4, 7, 4);
    put(v, 24 * 512 + 7 * 4, 0xffffffff, 4);
    const int root = 25 * 512;
    v[root] = char(0x81);
    put(v, root + 20, 3, 4);
    put(v, root + 24, 13, 8);
    // Clusters 2,3,4,5,7,8,9 allocated; deleted cluster 6 remains free.
    v[26 * 512] = char(0xef);
    const auto entry = [&](int at, const QString &name, uint32_t first,
                           uint64_t length, uint64_t valid, bool contiguous,
                           bool deleted = false, bool directory = false) {
      const int names = (name.size() + 14) / 15;
      QByteArray b((2 + names) * 32, char{0});
      b[0] = char(0x85);
      b[1] = char(1 + names);
      put(b, 4, directory ? 0x10 : 0x20);
      b[32] = char(0xc0);
      b[33] = char(contiguous ? 3 : 1);
      b[35] = char(name.size());
      put(b, 40, valid, 8);
      put(b, 52, first, 4);
      put(b, 56, length, 8);
      for (int i = 0; i < names; ++i)
        b[64 + i * 32] = char(0xc1);
      for (int i = 0; i < name.size(); ++i)
        put(b, 64 + (i / 15) * 32 + 2 + (i % 15) * 2, name[i].unicode());
      uint16_t checksum = 0;
      for (int i = 0; i < b.size(); ++i)
        if (i != 2 && i != 3)
          checksum = uint16_t((checksum >> 1) + ((checksum & 1) ? 0x8000 : 0) +
                              uint8_t(b[i]));
      put(b, 2, checksum);
      if (deleted)
        for (int i = 0; i < b.size(); i += 32)
          b[i] = char(uint8_t(b[i]) & 0x7f);
      v.replace(at, b.size(), b);
    };
    entry(root + 32, QStringLiteral("notes.txt"), 4, 8, 5, true);
    entry(root + 128, QStringLiteral("fragment.bin"), 5, 515, 515, false);
    entry(root + 224, QStringLiteral("gone.txt"), 6, 5, 5, true, true);
    entry(root + 320, QStringLiteral("folder"), 8, 512, 512, true, false, true);
    entry(31 * 512, QString::fromUtf8("résumé.txt"), 9, 6, 6, true);
    v.replace(27 * 512, 5, "hello");
    v.replace(28 * 512, 512, QByteArray(512, 'A'));
    v.replace(29 * 512, 5, "later");
    v.replace(30 * 512, 3, "end");
    v.replace(32 * 512, 6, "nested");
    uint32_t checksum = 0;
    for (int i = 0; i < 11 * 512; ++i)
      if (i != 106 && i != 107 && i != 112)
        checksum = (checksum >> 1) + ((checksum & 1) ? 0x80000000u : 0) +
                   uint8_t(v[i]);
    for (int i = 11 * 512; i < 12 * 512; i += 4)
      put(v, i, checksum, 4);
    return QByteArray(32 * 512, char{0}) + v;
  }
  static QByteArray ext4Disk() {
    QByteArray v(128 * 1024, char{0});
    const int sb = 1024;
    put(v, sb, 16, 4);
    put(v, sb + 4, 128, 4);
    put(v, sb + 20, 1, 4);
    put(v, sb + 32, 128, 4);
    put(v, sb + 40, 16, 4);
    put(v, sb + 56, 0xef53);
    put(v, sb + 88, 128);
    put(v, sb + 96, 0x42, 4);
    put(v, 2048, 3, 4);
    put(v, 2052, 4, 4);
    put(v, 2056, 5, 4);
    // Filesystem blocks 1..8 allocated; inodes 2 and 11 allocated.
    v[3 * 1024] = char(0xff);
    put(v, 4 * 1024, 0x402);
    const auto ino = [&](int number, int mode, int size, int physical) {
      const int at = 5 * 1024 + (number - 1) * 128;
      put(v, at, mode);
      put(v, at + 4, size, 4);
      put(v, at + 26, 1);
      put(v, at + 32, 0x80000, 4);
      put(v, at + 40, 0xf30a);
      put(v, at + 42, 1);
      put(v, at + 44, 4);
      put(v, at + 56, 1);
      put(v, at + 60, physical, 4);
    };
    ino(2, 0x41ed, 1024, 7);
    ino(11, 0x81a4, 5, 8);
    const int dir = 7 * 1024;
    put(v, dir, 2, 4);
    put(v, dir + 4, 12);
    v[dir + 6] = 1;
    v[dir + 7] = 2;
    v[dir + 8] = '.';
    put(v, dir + 12, 2, 4);
    put(v, dir + 16, 12);
    v[dir + 18] = 2;
    v[dir + 19] = 2;
    v.replace(dir + 20, 2, "..");
    put(v, dir + 24, 11, 4);
    put(v, dir + 28, 1000);
    v[dir + 30] = 9;
    v[dir + 31] = 1;
    v.replace(dir + 32, 9, "notes.txt");
    v.replace(8 * 1024, 5, "hello");
    return QByteArray(32 * 512, char{0}) + v;
  }
  static uint32_t crc(uint32_t c, const char *bytes, int n) {
    for (int i = 0; i < n; ++i) {
      c ^= uint8_t(bytes[i]);
      for (int b = 0; b < 8; ++b)
        c = (c >> 1) ^ ((c & 1) ? 0x82f63b78u : 0);
    }
    return c;
  }
  static QByteArray checksummedExt4Disk() {
    auto image = ext4Disk();
    const int base = 32 * 512, sb = base + 1024;
    put(image, sb + 100, 0x400, 4);
    image[sb + 373] = 1;
    const auto seed = crc(0xffffffffu, image.constData() + sb + 104, 16);
    const auto inodeSeed = [&](int number) {
      QByteArray n(8, char{0});
      put(n, 0, number, 4);
      return crc(seed, n.constData(), 8);
    };
    // Leave room for ext4_dir_entry_tail, then checksum the directory payload.
    const int dir = base + 7 * 1024;
    put(image, dir + 28, 988);
    put(image, dir + 1012, 0, 4);
    put(image, dir + 1016, 12);
    image[dir + 1018] = 0;
    image[dir + 1019] = char(0xde);
    put(image, dir + 1020, crc(inodeSeed(2), image.constData() + dir, 1012), 4);
    for (int number : {2, 11}) {
      const int at = base + 5 * 1024 + (number - 1) * 128;
      put(image, at + 124,
          crc(inodeSeed(number), image.constData() + at, 128) & 0xffff);
    }
    const int descriptor = base + 2048;
    put(image, descriptor + 24,
        crc(seed, image.constData() + base + 3 * 1024, 16) & 0xffff);
    put(image, descriptor + 26,
        crc(seed, image.constData() + base + 4 * 1024, 2) & 0xffff);
    QByteArray group(4, char{0});
    put(image, descriptor + 30,
        crc(crc(seed, group.constData(), 4), image.constData() + descriptor,
            32) &
            0xffff);
    put(image, sb + 1020, crc(0xffffffffu, image.constData() + sb, 1020), 4);
    return image;
  }
  static QByteArray indexedExt4Disk(bool checksums, bool internal,
                                    int fault = 0) {
    auto image = checksums ? checksummedExt4Disk() : ext4Disk();
    const int base = 32 * 512, root = base + 7 * 1024,
              ino = base + 5 * 1024 + 128;
    const auto seed =
        crc(0xffffffffu, image.constData() + base + 1024 + 104, 16);
    QByteArray identity(8, 0);
    put(identity, 0, 2, 4);
    const auto inodeSeed = crc(seed, identity.constData(), 8);
    put(image, ino + 32, 0x81000, 4);
    put(image, ino + 4, (internal ? 3 : 2) * 1024, 4);
    put(image, ino + 42, 2);
    put(image, ino + 64, 1, 4);
    put(image, ino + 68, internal ? 2 : 1);
    put(image, ino + 72, 9, 4);
    image[base + 3 * 1024 + 1] = char(internal ? 3 : 1);
    image.replace(root, 1024, QByteArray(1024, 0));
    put(image, root, 2, 4);
    put(image, root + 4, 12);
    image[root + 6] = 1;
    image[root + 7] = 2;
    image[root + 8] = '.';
    put(image, root + 12, 2, 4);
    put(image, root + 16, 1012);
    image[root + 18] = 2;
    image[root + 19] = 2;
    image.replace(root + 20, 2, "..");
    image[root + 28] = 1;
    image[root + 29] = 8;
    image[root + 30] = char(internal ? 1 : 0);
    put(image, root + 32, checksums ? 123 : 124);
    put(image, root + 34, 1);
    put(image, root + 36, 1, 4);
    const int leaf = base + (internal ? 10 : 9) * 1024;
    put(image, leaf, 11, 4);
    put(image, leaf + 4, checksums ? 1012 : 1024);
    image[leaf + 6] = 9;
    image[leaf + 7] = 1;
    image.replace(leaf + 8, 9, "notes.txt");
    const int node = base + 9 * 1024;
    if (internal) {
      put(image, node + 4, 1024);
      put(image, node + 8, checksums ? 126 : 127);
      put(image, node + 10, 1);
      put(image, node + 12, 2, 4);
    }
    if (fault == 1)
      put(image, root + 36, 0, 4); // cyclic root reference
    if (fault == 2)
      put(image, root + 36, 99, 4); // outside directory
    if (fault == 3)
      put(image, root + 34, 200); // excessive count
    if (fault == 4)
      image[root + 30] = 3; // unsupported depth
    if (fault == 5 && internal)
      put(image, node + 12, 1, 4); // cyclic internal node
    if (fault == 6) {
      put(image, root + 34, 2);
      put(image, root + 40, 2, 4);
      put(image, root + 44, 1, 4);
    } // aliased child
    const auto indexChecksum = [&](int at, int offset) {
      const int count = uint8_t(image[at + offset + 2]) |
                        (int(uint8_t(image[at + offset + 3])) << 8);
      // Deliberately malformed counts still get a checksum over their bounded
      // bytes.
      if (count > 123)
        return;
      auto digest = crc(inodeSeed, image.constData() + at, offset + count * 8);
      QByteArray tail(8, 0);
      digest = crc(digest, tail.constData(), 8);
      put(image, at + 1020, digest, 4);
    };
    if (checksums) {
      indexChecksum(root, 32);
      if (internal)
        indexChecksum(node, 8);
      put(image, leaf + 1016, 12);
      image[leaf + 1019] = char(0xde);
      put(image, leaf + 1020, crc(inodeSeed, image.constData() + leaf, 1012),
          4);
      put(image, ino + 124, 0);
      put(image, ino + 124,
          crc(inodeSeed, image.constData() + ino, 128) & 0xffff);
      const int descriptor = base + 2048;
      put(image, descriptor + 24,
          crc(seed, image.constData() + base + 3 * 1024, 16) & 0xffff);
      put(image, descriptor + 30, 0);
      QByteArray group(4, 0);
      put(image, descriptor + 30,
          crc(crc(seed, group.constData(), 4), image.constData() + descriptor,
              32) &
              0xffff);
      if (fault == 7)
        image[root + 1020] = char(uint8_t(image[root + 1020]) ^ 1);
      if (fault == 8 && internal)
        image[node + 1020] = char(uint8_t(image[node + 1020]) ^ 1);
      if (fault == 9)
        image[leaf + 20] = 1;
    }
    return image;
  }
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
  static models::RecoveryOperation scan(const models::StorageDevice &source) {
    models::RecoveryOperation op{};
    op.operationType = "disk_scan";
    op.targetDevicePath = source.devicePath;
    op.numSectors = source.totalBytes / source.bytesPerSector;
    return op;
  }
  static void snapshot(QWidget &widget, const QString &name) {
    const auto workspace = qEnvironmentVariable("GITHUB_WORKSPACE");
    if (qEnvironmentVariable("CI") != "true" || workspace.isEmpty())
      return;
    const auto directory = workspace + "/build/screenshots";
    if (!QDir().mkpath(directory) ||
        !widget.grab().save(directory + "/" + name + ".png"))
      throw std::runtime_error("Cannot save CI workflow screenshot");
  }
private slots:
  void lostPartitionScanAndSelectiveRecovery() {
    QTemporaryDir root;
    const auto path = root.filePath("whole-disk.img");
    const auto bytes = disk();
    save(path, bytes);
    GUIRecoveryService service(nullptr);
    auto op = scan(service.openImage(path.toStdString()));
    const auto catalog = service.executeOperation(op);
    QVERIFY2(catalog.success, catalog.errorMessage.c_str());
    QVERIFY(catalog.scanOnly);
    QVERIFY(!catalog.outputWritten);
    QCOMPARE(catalog.details.at("Filesystem volumes found"), std::string("1"));
    const std::string live = "FAT12_32/Existing/HELLO.TXT";
    bool foundLive = false, foundDeleted = false, foundSignature = false;
    for (const auto &item : catalog.recoveredItems) {
      foundLive |= item.name == live;
      foundDeleted |= item.name.find("/Deleted/") != std::string::npos;
      foundSignature |= item.name.starts_with("Signature matches/");
      QVERIFY(!item.confidence.empty());
    }
    QCOMPARE(service.previewCandidate(live),
             std::vector<uint8_t>({'h', 'e', 'l', 'l', 'o'}));
    QVERIFY_EXCEPTION_THROWN(service.previewCandidate("../not-in-catalog"),
                             std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(
        service.previewCandidate(live, [] { return true; }),
        std::runtime_error);
    QVERIFY(foundLive);
    QVERIFY(foundDeleted);
    QVERIFY(foundSignature);
    QCOMPARE(QDir(root.path()).entryList(QDir::Files).size(), 1);
    QVERIFY(QDir().mkdir(root.filePath("recovered")));
    op.operationType = "disk_recovery";
    op.outputPath = root.filePath("recovered").toStdString();
    op.selectedPaths = {live};
    const auto recovered = service.executeOperation(op);
    QVERIFY2(recovered.success, recovered.errorMessage.c_str());
    QCOMPARE(recovered.recoveredItemsCount, uint64_t{1});
    QCOMPARE(read(root.filePath("recovered/") + QString::fromStdString(live)),
             QByteArray("hello"));
    QCOMPARE(read(path), bytes);
    const auto duplicate = service.executeOperation(op);
    QVERIFY(!duplicate.success);
    QCOMPARE(duplicate.failedItemsCount, uint64_t{1});
    op.selectedPaths = {"../not-in-catalog"};
    QVERIFY(!service.executeOperation(op).success);
    QCOMPARE(read(path), bytes);
  }
  void damagedFilesystemStillFindsSignatures() {
    QTemporaryDir root;
    const auto path = root.filePath("damaged.img");
    save(path, disk(true));
    GUIRecoveryService service(nullptr);
    const auto op = scan(service.openImage(path.toStdString()));
    const auto result = service.executeOperation(op);
    QVERIFY2(result.success, result.errorMessage.c_str());
    QCOMPARE(result.details.at("Filesystem volumes found"), std::string("0"));
    QCOMPARE(result.recoveredItemsCount, uint64_t{1});
    QVERIFY(result.recoveredItems[0].name.starts_with("Signature matches/"));
    QVERIFY(!result.outputWritten);
  }
  void cancelledScanDoesNotWriteAndNewSourceInvalidatesCatalog() {
    QTemporaryDir root;
    save(root.filePath("disk.img"), disk());
    save(root.filePath("other.img"), disk());
    GUIRecoveryService service(nullptr);
    auto op = scan(service.openImage(root.filePath("disk.img").toStdString()));
    const auto cancelled =
        service.executeOperation(op, {}, [] { return true; });
    QVERIFY(cancelled.cancelled);
    QVERIFY(!cancelled.outputWritten);
    QVERIFY(service.executeOperation(op).success);
    service.openImage(root.filePath("other.img").toStdString());
    op.targetDevicePath = root.filePath("other.img").toStdString();
    op.operationType = "disk_recovery";
    op.outputPath = root.path().toStdString();
    op.selectedPaths = {"FAT12_32/Existing/HELLO.TXT"};
    QVERIFY(!service.executeOperation(op).success);
  }
  void previewIsBoundedAndRejectsChangedSource() {
    QTemporaryDir root;
    auto volume = disk().mid(32 * 512, 64 * 512);
    volume += QByteArray(192 * 512, char{0});
    put(volume, 19, 256);
    volume[3 * 512 + 32] = 0;
    for (int cluster = 2; cluster <= 138; ++cluster) {
      const uint16_t next = cluster == 138 ? 0xfff : uint16_t(cluster + 1);
      for (int base : {512, 1024}) {
        const int at = base + cluster + cluster / 2;
        uint16_t word =
            uint8_t(volume[at]) | (uint16_t(uint8_t(volume[at + 1])) << 8);
        word = cluster & 1 ? uint16_t((word & 0xf) | (next << 4))
                           : uint16_t((word & 0xf000) | next);
        put(volume, at, word);
      }
    }
    QByteArray payload(70000, 'x');
    payload[0] = 'D';
    payload[65535] = '!';
    put(volume, 3 * 512 + 28, 70000, 4);
    volume.replace(4 * 512, payload.size(), payload);
    const auto original = QByteArray(32 * 512, char{0}) + volume;
    const auto path = root.filePath("large.img");
    save(path, original);
    GUIRecoveryService service(nullptr);
    const auto result =
        service.executeOperation(scan(service.openImage(path.toStdString())));
    QVERIFY2(result.success, result.errorMessage.c_str());
    const auto preview =
        service.previewCandidate("FAT12_32/Existing/HELLO.TXT");
    QCOMPARE(preview.size(), size_t(65536));
    QCOMPARE(QByteArray(reinterpret_cast<const char *>(preview.data()),
                        qsizetype(preview.size())),
             payload.left(65536));
    QCOMPARE(read(path), original);
    QCOMPARE(QDir(root.path()).entryList(QDir::Files).size(), 1);
    QFile mutation(path);
#ifdef _WIN32
    // Native Windows read-only handles deny concurrent writes. Verify that
    // protection rather than trying to bypass it to construct this fixture.
    QVERIFY(!mutation.open(QIODevice::WriteOnly | QIODevice::Append));
    QCOMPARE(read(path), original);
    QCOMPARE(service.previewCandidate("FAT12_32/Existing/HELLO.TXT"), preview);
#else
    QVERIFY(mutation.open(QIODevice::WriteOnly | QIODevice::Append));
    QCOMPARE(mutation.write(QByteArray(512, 'z')), qint64(512));
    mutation.close();
    QVERIFY_EXCEPTION_THROWN(
        service.previewCandidate("FAT12_32/Existing/HELLO.TXT"),
        std::runtime_error);
#endif
  }
  void exfatLostVolumeAndSelectedRecovery() {
    QTemporaryDir root;
    const auto path = root.filePath("exfat.img");
    const auto original = exfatDisk();
    save(path, original);
    GUIRecoveryService service(nullptr);
    auto op = scan(service.openImage(path.toStdString()));
    const auto catalog = service.executeOperation(op);
    QVERIFY2(catalog.success, catalog.errorMessage.c_str());
    QCOMPARE(catalog.details.at("Filesystem volumes found"), std::string("1"));
    QCOMPARE(catalog.recoveredItemsCount, uint64_t{4});
    QVERIFY(!catalog.outputWritten);
    QVERIFY(QDir().mkdir(root.filePath("out")));
    op.operationType = "disk_recovery";
    op.outputPath = root.filePath("out").toStdString();
    for (const auto &file : catalog.recoveredItems)
      op.selectedPaths.push_back(file.name);
    const auto result = service.executeOperation(op);
    QVERIFY2(result.success, result.errorMessage.c_str());
    QCOMPARE(result.recoveredItemsCount, uint64_t{4});
    QCOMPARE(read(root.filePath("out/exFAT_32/Existing/notes.txt")),
             QByteArray("hello\0\0\0", 8));
    QCOMPARE(read(root.filePath("out/exFAT_32/Existing/fragment.bin")),
             QByteArray(512, 'A') + "end");
    QCOMPARE(read(root.filePath("out/exFAT_32/Deleted/gone.txt")),
             QByteArray("later"));
    QCOMPARE(read(root.filePath(
                 QString::fromUtf8("out/exFAT_32/Existing/folder/résumé.txt"))),
             QByteArray("nested"));
    QCOMPARE(read(path), original);
  }
  void exfatRejectsCrossLinkedFilesAndMetadataAliases() {
    for (bool metadataAlias : {false, true}) {
      QTemporaryDir root;
      auto bytes = exfatDisk();
      const int at = 32 * 512 + 25 * 512 + (metadataAlias ? 32 : 128);
      put(bytes, at + 52, metadataAlias ? 8 : 4, 4);
      bytes[at + 33] = 3;
      put(bytes, at + 40, 5, 8);
      put(bytes, at + 56, 8, 8);
      uint16_t checksum = 0;
      for (int i = 0; i < 96; ++i)
        if (i != 2 && i != 3)
          checksum = uint16_t((checksum >> 1) + ((checksum & 1) ? 0x8000 : 0) +
                              uint8_t(bytes[at + i]));
      put(bytes, at + 2, checksum);
      const auto path = root.filePath("alias.img");
      save(path, bytes);
      GUIRecoveryService service(nullptr);
      const auto result =
          service.executeOperation(scan(service.openImage(path.toStdString())));
      QVERIFY2(result.success, result.errorMessage.c_str());
      QCOMPARE(result.recoveredItemsCount, uint64_t(metadataAlias ? 3 : 2));
      for (const auto &file : result.recoveredItems)
        QVERIFY(!file.name.ends_with("notes.txt"));
      QCOMPARE(read(path), bytes);
    }
  }
  void exfatRejectsCorruptMetadata_data() {
    QTest::addColumn<int>("offset");
    QTest::addColumn<int>("value");
    QTest::addColumn<int>("expected");
    QTest::newRow("boot-checksum") << 11 * 512 << 1 << 0;
    QTest::newRow("entry-checksum") << 25 * 512 + 32 + 2 << 1 << 3;
    QTest::newRow("deleted-cluster-reallocated") << 26 * 512 << 0xff << 3;
    QTest::newRow("fragment-chain-cycle") << 24 * 512 + 5 * 4 << 5 << 3;
    QTest::newRow("missing-bitmap") << 25 * 512 << 0 << 0;
    QTest::newRow("directory-cycle") << 25 * 512 + 320 + 52 << 2 << 3;
  }
  void exfatBackupBootRecoveryAndValidation() {
    for (bool rawImage : {false, true})
      for (int fault = 0; fault < 5; ++fault) {
        QTemporaryDir root;
        auto bytes = exfatDisk();
        const int base = 32 * 512;
        const auto checksum = [&](int start) {
          uint32_t value = 0;
          for (int i = 0; i < 11 * 512; ++i)
            if (i != 106 && i != 107 && i != 112)
              value = (value >> 1) + ((value & 1) ? 0x80000000u : 0) +
                      uint8_t(bytes[start + i]);
          for (int i = 11 * 512; i < 12 * 512; i += 4)
            put(bytes, start + i, value, 4);
        };
        put(bytes, base + 64, 32, 8);
        checksum(base);
        bytes.replace(base + 12 * 512, 12 * 512, bytes.mid(base, 12 * 512));
        if (fault == 1)
          bytes.replace(base, 12 * 512, QByteArray(12 * 512, 0));
        if (fault == 2)
          bytes[base + 110] = 2; // geometry and checksum both invalid
        if (fault == 3) {
          bytes[base + 511] = 0;
          bytes[base + 23 * 512] = char(uint8_t(bytes[base + 23 * 512]) ^ 1);
        }
        if (fault == 4) {
          bytes[base + 110] = 2;
          checksum(base);
        } // checksum-valid unsupported primary must not use stale backup
        if (rawImage)
          bytes.remove(0, base);
        const auto path = root.filePath("exfat-backup.img");
        save(path, bytes);
        GUIRecoveryService service(nullptr);
        auto op = scan(service.openImage(path.toStdString()));
        op.deepSearch = false;
        const auto result = service.executeOperation(op);
        QVERIFY2(result.success, result.errorMessage.c_str());
        QCOMPARE(result.recoveredItemsCount, uint64_t(fault >= 3 ? 0 : 4));
        if (fault < 3) {
          const std::string name = std::string("exFAT_") +
                                   (rawImage ? "0" : "32") +
                                   "/Deleted/gone.txt";
          QCOMPARE(service.previewCandidate(name),
                   std::vector<uint8_t>({'l', 'a', 't', 'e', 'r'}));
          if (fault) {
            QVERIFY(result.details.contains("Scan issue 1"));
            QVERIFY(result.details.at("Scan issue 1").find("backup geometry") !=
                    std::string::npos);
          }
          service.saveScan(root.filePath("backup.rscan").toStdString());
          QVERIFY(service.loadScan(root.filePath("backup.rscan").toStdString())
                      .success);
          QVERIFY(QDir().mkdir(root.filePath("out")));
          op.operationType = "disk_recovery";
          op.selectedPaths = {name};
          op.outputPath = root.filePath("out").toStdString();
          QVERIFY(service.executeOperation(op).success);
          QCOMPARE(read(root.filePath("out/" + QString::fromStdString(name))),
                   QByteArray("later"));
        }
        QCOMPARE(read(path), bytes);
      }
  }
  void exfatRejectsCorruptMetadata() {
    QFETCH(int, offset);
    QFETCH(int, value);
    QFETCH(int, expected);
    QTemporaryDir root;
    auto bytes = exfatDisk();
    bytes[32 * 512 + offset] = char(value);
    const auto path = root.filePath("bad.img");
    save(path, bytes);
    GUIRecoveryService service(nullptr);
    const auto result =
        service.executeOperation(scan(service.openImage(path.toStdString())));
    QVERIFY2(result.success, result.errorMessage.c_str());
    QCOMPARE(result.recoveredItemsCount, uint64_t(expected));
    QVERIFY(!result.outputWritten);
    QCOMPARE(read(path), bytes);
  }
  void ext4LostVolumeAndSelectedRecovery() {
    QTemporaryDir root;
    const auto path = root.filePath("ext4.img");
    const auto original = ext4Disk();
    save(path, original);
    GUIRecoveryService service(nullptr);
    auto op = scan(service.openImage(path.toStdString()));
    const auto result = service.executeOperation(op);
    QVERIFY2(result.success, result.errorMessage.c_str());
    QCOMPARE(result.details.at("Filesystem volumes found"), std::string("1"));
    QCOMPARE(result.recoveredItemsCount, uint64_t(1));
    QCOMPARE(result.recoveredItems[0].name,
             std::string("ext4_32/Existing/notes.txt"));
    QCOMPARE(service.previewCandidate(result.recoveredItems[0].name),
             std::vector<uint8_t>({'h', 'e', 'l', 'l', 'o'}));
    QVERIFY(result.details.contains("Scan issue 1"));
    service.saveScan(root.filePath("ext4.rscan").toStdString());
    const auto loaded =
        service.loadScan(root.filePath("ext4.rscan").toStdString());
    QVERIFY(loaded.details.contains("Scan issue 1"));
    QCOMPARE(loaded.details.at("Scan issue 1"),
             result.details.at("Scan issue 1"));
    auto continuedOp = op;
    continuedOp.resumeScan = true;
    const auto continued = service.executeOperation(continuedOp);
    QVERIFY(continued.success);
    QVERIFY(continued.details.contains("Scan issue 1"));
    QCOMPARE(continued.details.at("Scan issue 1"),
             result.details.at("Scan issue 1"));
    QVERIFY(QDir().mkdir(root.filePath("out")));
    op.operationType = "disk_recovery";
    op.outputPath = root.filePath("out").toStdString();
    op.selectedPaths = {result.recoveredItems[0].name};
    const auto recovered = service.executeOperation(op);
    QVERIFY2(recovered.success, recovered.errorMessage.c_str());
    QCOMPARE(read(root.filePath("out/ext4_32/Existing/notes.txt")),
             QByteArray("hello"));
    QCOMPARE(read(path), original);
  }
  void ext4IndexedDirectoriesAndCorruption() {
    for (bool checksums : {false, true})
      for (bool internal : {false, true})
        for (int fault = 0; fault <= 9; ++fault) {
          if ((fault == 5 || fault == 8) && !internal)
            continue;
          if (fault >= 7 && !checksums)
            continue;
          QTemporaryDir root;
          const auto path = root.filePath("indexed.img");
          const auto bytes = indexedExt4Disk(checksums, internal, fault);
          save(path, bytes);
          GUIRecoveryService service(nullptr);
          auto op = scan(service.openImage(path.toStdString()));
          op.deepSearch = false;
          const auto result = service.executeOperation(op);
          QVERIFY2(result.success, result.errorMessage.c_str());
          QVERIFY2(result.recoveredItemsCount == uint64_t(fault ? 0 : 1),
                   qPrintable(QString("checksums=%1 depth=%2 fault=%3 count=%4")
                                  .arg(checksums)
                                  .arg(internal)
                                  .arg(fault)
                                  .arg(result.recoveredItemsCount)));
          if (!fault) {
            QCOMPARE(result.recoveredItems[0].name,
                     std::string("ext4_32/Existing/notes.txt"));
            QCOMPARE(service.previewCandidate(result.recoveredItems[0].name),
                     std::vector<uint8_t>({'h', 'e', 'l', 'l', 'o'}));
          }
          QCOMPARE(read(path), bytes);
        }
  }
  void ext4MetadataChecksumsRejectCorruption() {
    QCOMPARE(
        recovery::ext4::crc32c(
            0xffffffffu, reinterpret_cast<const uint8_t *>("123456789"), 9) ^
            0xffffffffu,
        uint32_t(0xe3069283));
    for (const int offset :
         {-1, 1024 + 1020, 2048 + 30, 3 * 1024 + 5, 4 * 1024 + 1,
          5 * 1024 + 10 * 128 + 10, 7 * 1024 + 33}) {
      QTemporaryDir root;
      auto bytes = checksummedExt4Disk();
      if (offset >= 0)
        bytes[32 * 512 + offset] = char(uint8_t(bytes[32 * 512 + offset]) ^ 1);
      const auto path = root.filePath("checksum.img");
      save(path, bytes);
      GUIRecoveryService service(nullptr);
      auto op = scan(service.openImage(path.toStdString()));
      op.deepSearch = false;
      const auto result = service.executeOperation(op);
      QVERIFY2(result.success, result.errorMessage.c_str());
      QCOMPARE(result.recoveredItemsCount, uint64_t(offset < 0 ? 1 : 0));
      if (offset < 0)
        QVERIFY(result.recoveredItems[0].confidence.find(
                    "checksums and allocation checked") != std::string::npos);
      QCOMPARE(read(path), bytes);
    }
  }
  void ext4RejectsInvalidExtentsAndAllocation_data() {
    QTest::addColumn<int>("offset");
    QTest::addColumn<int>("value");
    QTest::newRow("unallocated-data") << 3 * 1024 << 0x7f;
    QTest::newRow("metadata-alias") << 5 * 1024 + 10 * 128 + 60 << 5;
    QTest::newRow("extent-outside-disk") << 5 * 1024 + 10 * 128 + 60 << 255;
    QTest::newRow("invalid-extent-header") << 5 * 1024 + 10 * 128 + 40 << 0;
    QTest::newRow("directory-record-length") << 7 * 1024 + 28 << 1;
  }
  void ext4RejectsInvalidExtentsAndAllocation() {
    QFETCH(int, offset);
    QFETCH(int, value);
    QTemporaryDir root;
    auto bytes = ext4Disk();
    bytes[32 * 512 + offset] = char(value);
    const auto path = root.filePath("bad.img");
    save(path, bytes);
    GUIRecoveryService service(nullptr);
    const auto result =
        service.executeOperation(scan(service.openImage(path.toStdString())));
    QVERIFY2(result.success, result.errorMessage.c_str());
    QCOMPARE(result.recoveredItemsCount, uint64_t(0));
    QVERIFY(!result.outputWritten);
    QCOMPARE(read(path), bytes);
  }
  void volumeDiscoverySkipsKnownVolumePayload() {
    class CountingSource : public disk::IDiskReader {
      disk::DiskImageReader image_;

    public:
      uint64_t sectorsRead{};
      void open(const std::string &path) override { image_.open(path); }
      void close() override { image_.close(); }
      bool isOpen() const noexcept override { return image_.isOpen(); }
      uint32_t getSectorSize() const noexcept override {
        return image_.getSectorSize();
      }
      uint64_t getTotalSectors() const noexcept override {
        return image_.getTotalSectors();
      }
      disk::DeviceInformation getDiskInfo() const override {
        return image_.getDiskInfo();
      }
      bool writeSectors(uint64_t a, uint64_t b,
                        const std::vector<std::byte> &c) override {
        return image_.writeSectors(a, b, c);
      }
      bool readSectors(uint64_t a, uint64_t b,
                       std::vector<std::byte> &c) override {
        sectorsRead += b;
        return image_.readSectors(a, b, c);
      }
    };
    QTemporaryDir root;
    const auto path = root.filePath("ext4.img");
    save(path, ext4Disk());
    CountingSource source;
    source.open(path.toStdString());
    const auto volumes = recovery::findDiskVolumes(source, {}, {});
    QCOMPARE(volumes.size(), size_t(1));
    QCOMPARE(volumes[0].start, uint64_t(32));
    QVERIFY(source.sectorsRead <= 130);
    QVERIFY(source.sectorsRead < source.getTotalSectors());
  }
  void filesystemOnlyScanPreservesDepthAcrossSaveAndRecovery() {
    QTemporaryDir root;
    const auto path = root.filePath("source.img");
    save(path, disk());
    GUIRecoveryService service(nullptr);
    auto op = scan(service.openImage(path.toStdString()));
    op.deepSearch = false;
    const auto result = service.executeOperation(op);
    QVERIFY2(result.success, result.errorMessage.c_str());
    QCOMPARE(result.recoveredItemsCount, uint64_t(2));
    for (const auto &file : result.recoveredItems)
      QVERIFY(!file.name.starts_with("Signature matches/"));
    service.saveScan(root.filePath("quick.rscan").toStdString());
    const auto restored =
        service.loadScan(root.filePath("quick.rscan").toStdString());
    QCOMPARE(restored.recoveredItemsCount, uint64_t(2));
    QVERIFY(QDir().mkdir(root.filePath("out")));
    op.operationType = "disk_recovery";
    op.outputPath = root.filePath("out").toStdString();
    op.selectedPaths = {"FAT12_32/Existing/HELLO.TXT"};
    const auto recovered = service.executeOperation(op);
    QVERIFY2(recovered.success, recovered.errorMessage.c_str());
    QCOMPARE(read(root.filePath("out/FAT12_32/Existing/HELLO.TXT")),
             QByteArray("hello"));
    QVERIFY(recovered.details.at("Search depth").find("skipped") !=
            std::string::npos);
  }
  void rescueImageGuiFlow() {
    QTemporaryDir root;
    const auto source = root.filePath("source.img"),
               target = root.filePath("rescue.img");
    save(source, disk());
    GUIRecoveryService service(nullptr);
    gui::core::MainWindow window(&service);
    window.show();
    window.findChild<gui::widgets::SourceDiscoveryWidget *>()->openImagePath(
        source);
    window.rescueImageAs(target);
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(target + ".rescue.json"), 10000);
    QTRY_VERIFY(
        window.findChild<QPushButton *>("recoverySettings")->isEnabled());
    QCOMPARE(read(target), disk());
    QCOMPARE(QJsonDocument::fromJson(read(target + ".rescue.json"))
                 .object()["status"]
                 .toString(),
             QString("completed"));
    window.close();
  }
  void authorizedDeviceIntegration() {
#ifdef __linux__
    const auto path = qEnvironmentVariable("RS_AUTHORIZED_DEVICE");
    if (path.isEmpty())
      QSKIP("Dedicated CI loop-device authorization fixture only");
    const auto descriptor = gui::core::authorizeReadOnlyDevice(path);
    QVERIFY(descriptor.isValid());
    disk::ReadOnlyDevice source;
    source.openAuthorized(path.toStdString(), descriptor.fileDescriptor());
    QVERIFY(source.isOpen());
    QVERIFY(source.getDiskInfo().isReadOnly);
    std::vector<std::byte> bytes;
    QVERIFY(source.readSectors(0, 1, bytes));
    QCOMPARE(bytes.size(), size_t(source.getSectorSize()));
    QVERIFY_EXCEPTION_THROWN(source.writeSectors(0, 1, bytes),
                             disk::AccessDeniedException);
    QTemporaryDir root;
    save(root.filePath("fake"), QByteArray(512, 0));
    QVERIFY_EXCEPTION_THROWN(
        source.openAuthorized(root.filePath("fake").toStdString(),
                              descriptor.fileDescriptor()),
        disk::DiskException);
    QVERIFY(source.isOpen());
#else
    QSKIP("UDisks2 descriptor transport is Linux-only");
#endif
  }
  void rasterPreviewDecodesOffscreenAndRemainsReadOnly() {
    QTemporaryDir root;
    QImage fixture(32, 24, QImage::Format_RGB32);
    fixture.fill(Qt::green);
    QByteArray png;
    QBuffer buffer(&png);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(fixture.save(&buffer, "PNG"));
    QVERIFY(png.size() < 512);
    auto bytes = disk();
    put(bytes, 32 * 512 + 3 * 512 + 28, uint64_t(png.size()), 4);
    bytes.replace(32 * 512 + 4 * 512, png.size(), png);
    const auto path = root.filePath("source.img");
    save(path, bytes);
    GUIRecoveryService service(nullptr);
    auto op = scan(service.openImage(path.toStdString()));
    QVERIFY(service.executeOperation(op).success);
    op.operationType = "disk_preview";
    op.selectedPaths = {"FAT12_32/Existing/HELLO.TXT"};
    gui::core::RecoveryWorker worker(&service, op);
    models::RecoveryResult result;
    connect(&worker, &gui::core::RecoveryWorker::finished, this,
            [&](const auto &r) { result = r; });
    worker.process();
    QVERIFY2(result.success, result.errorMessage.c_str());
    QVERIFY(!result.previewImagePNG.empty());
    QVERIFY(result.previewBytes.size() <= 65536);
    QImage image;
    QVERIFY(image.loadFromData(result.previewImagePNG.data(),
                               int(result.previewImagePNG.size()), "PNG"));
    QVERIFY(image.width() <= 800 && image.height() <= 450);
    QCOMPARE(image.pixelColor(image.width() / 2, image.height() / 2),
             QColor(Qt::green));
    gui::widgets::RecoveryResultsWidget widget;
    widget.setPreview(result);
    widget.show();
    QVERIFY(widget.findChild<QLabel *>("candidateImagePreview")->isVisible());
    QCOMPARE(read(path), bytes);
    QVERIFY(!result.outputWritten);
    QVERIFY_EXCEPTION_THROWN(service.previewCandidate(op.selectedPaths.front(),
                                                      {}, 16 * 1024 * 1024 + 1),
                             std::runtime_error);
  }
  void interruptedScanPersistsAndContinuesAcrossServices() {
    QTemporaryDir root;
    const auto path = root.filePath("source.img"),
               snapshot = root.filePath("partial.rscan");
    auto bytes = disk();
    bytes.resize(1024 * 1024);
    bytes.fill(0, bytes.size());
    const auto original = disk();
    bytes.replace(0, original.size(), original);
    bytes.replace(100000, 5, "%PDF-");
    bytes.replace(200000, 5, "%%EOF");
    save(path, bytes);
    {
      GUIRecoveryService service(nullptr);
      auto op = scan(service.openImage(path.toStdString()));
      bool cancel = false;
      const auto partial = service.executeOperation(
          op,
          [&](const auto &p) {
            if (p.currentStep == "Searching for surviving and lost volumes")
              cancel = true;
          },
          [&] { return cancel; });
      QVERIFY(partial.cancelled);
      service.saveScan(snapshot.toStdString());
      const auto c = QJsonDocument::fromJson(read(snapshot))
                         .object()["checkpoint"]
                         .toObject();
      QVERIFY(c["volumeSector"].toString().toULongLong() > 0);
      QVERIFY(!c["volumesDone"].toBool());
    }
    const auto second = root.filePath("deep-partial.rscan");
    {
      GUIRecoveryService service(nullptr);
      QVERIFY(service.loadScan(snapshot.toStdString()).success);
      auto op = scan(service.getStorageDeviceInfo(path.toStdString()));
      op.resumeScan = true;
      bool cancel = false;
      const auto partial = service.executeOperation(
          op,
          [&](const auto &p) {
            if (p.currentStep == "Deep search for file signatures" &&
                p.processedBytes >= 131072)
              cancel = true;
          },
          [&] { return cancel; });
      QVERIFY(partial.cancelled);
      QVERIFY(partial.recoveredItemsCount >= 3);
      service.saveScan(second.toStdString());
      const auto c = QJsonDocument::fromJson(read(second))
                         .object()["checkpoint"]
                         .toObject();
      QVERIFY(c["volumesDone"].toBool());
      QCOMPARE(c["signatureOffset"].toString().toULongLong(), quint64(100000));
      QCOMPARE(c["signatureItems"].toString().toULongLong(), quint64(1));
    }
    GUIRecoveryService resumed(nullptr), fresh(nullptr);
    const auto partial = resumed.loadScan(second.toStdString());
    auto op = scan(resumed.getStorageDeviceInfo(path.toStdString()));
    op.resumeScan = true;
    const auto completed = resumed.executeOperation(op);
    QVERIFY2(completed.success, completed.errorMessage.c_str());
    const auto reference =
        fresh.executeOperation(scan(fresh.openImage(path.toStdString())));
    QVERIFY(reference.success);
    QCOMPARE(completed.recoveredItems.size(), reference.recoveredItems.size());
    for (size_t i = 0; i < reference.recoveredItems.size(); ++i) {
      QCOMPARE(completed.recoveredItems[i].name,
               reference.recoveredItems[i].name);
      QCOMPARE(completed.recoveredItems[i].sizeBytes,
               reference.recoveredItems[i].sizeBytes);
    }
    QCOMPARE(completed.recoveredItemsCount, partial.recoveredItemsCount + 1);
    QVERIFY(!completed.outputWritten);
    resumed.saveScan(root.filePath("completed.rscan").toStdString());
    auto bad = QJsonDocument::fromJson(read(second)).object();
    auto c = bad["checkpoint"].toObject();
    c["signatureOffset"] = "999999999";
    bad["checkpoint"] = c;
    save(root.filePath("bad-cursor.rscan"), QJsonDocument(bad).toJson());
    QVERIFY_EXCEPTION_THROWN(
        resumed.loadScan(root.filePath("bad-cursor.rscan").toStdString()),
        std::runtime_error);
  }
  void savedScanRoundTripAndValidation() {
    QTemporaryDir root;
    const auto path = root.filePath("source.img"),
               snapshot = root.filePath("saved.rscan");
    save(path, exfatDisk());
    {
      GUIRecoveryService service(nullptr);
      auto op = scan(service.openImage(path.toStdString()));
      QVERIFY(service.executeOperation(op).success);
      service.saveScan(snapshot.toStdString());
      QVERIFY_EXCEPTION_THROWN(service.saveScan(snapshot.toStdString()),
                               std::runtime_error);
      QVERIFY_EXCEPTION_THROWN(
          service.saveScan(root.filePath("cancelled.rscan").toStdString(),
                           [] { return true; }),
          std::runtime_error);
      QVERIFY(!QFile::exists(root.filePath("cancelled.rscan")));
    }
    GUIRecoveryService restored(nullptr);
    const auto result = restored.loadScan(snapshot.toStdString());
    QVERIFY(result.success);
    QVERIFY(result.scanOnly);
    QVERIFY(!result.outputWritten);
    QCOMPARE(result.recoveredItemsCount, uint64_t(4));
    QCOMPARE(restored.previewCandidate("exFAT_32/Deleted/gone.txt"),
             std::vector<uint8_t>({'l', 'a', 't', 'e', 'r'}));
    auto obj = QJsonDocument::fromJson(read(snapshot)).object();
    obj["sectors"] = "999999";
    save(root.filePath("bad.rscan"), QJsonDocument(obj).toJson());
    QVERIFY_EXCEPTION_THROWN(
        restored.loadScan(root.filePath("bad.rscan").toStdString()),
        std::runtime_error);
    QCOMPARE(restored.previewCandidate("exFAT_32/Deleted/gone.txt").size(),
             size_t(5));
    QVERIFY(QDir().mkdir(root.filePath("out")));
    auto op = scan(restored.getStorageDeviceInfo(path.toStdString()));
    op.operationType = "disk_recovery";
    op.outputPath = root.filePath("out").toStdString();
    op.selectedPaths = {"exFAT_32/Deleted/gone.txt"};
    QVERIFY(restored.executeOperation(op).success);
    QCOMPARE(read(root.filePath("out/exFAT_32/Deleted/gone.txt")),
             QByteArray("later"));
  }
  void savedScanRejectsChangedSource() {
    QTemporaryDir root;
    const auto path = root.filePath("disk.img"),
               snapshot = root.filePath("saved.rscan");
    save(path, disk());
    {
      GUIRecoveryService service(nullptr);
      QVERIFY(
          service.executeOperation(scan(service.openImage(path.toStdString())))
              .success);
      service.saveScan(snapshot.toStdString());
    }
    auto changed = disk();
    changed[32 * 512 + 4 * 512] = 'X';
    save(path, changed);
    GUIRecoveryService service(nullptr);
    QVERIFY_EXCEPTION_THROWN(service.loadScan(snapshot.toStdString()),
                             std::runtime_error);
    save(root.filePath("invalid.rscan"), QByteArray(16 * 1024 * 1024 + 1, ' '));
    QVERIFY_EXCEPTION_THROWN(
        service.loadScan(root.filePath("invalid.rscan").toStdString()),
        std::runtime_error);
  }
  void scanReviewRecoverGui() {
    QTemporaryDir root;
    const auto path = root.filePath("disk.img");
    save(path, disk());
    QVERIFY(QDir().mkdir(root.filePath("out")));
    GUIRecoveryService service(nullptr);
    gui::core::MainWindow window(&service);
    window.show();
    QTest::qWait(50);
    snapshot(window, "01-connected-drives");
    window.findChild<gui::widgets::SourceDiscoveryWidget *>()->openImagePath(
        path);
    window.findChild<QPushButton *>("startImageRecovery")->click();
    auto *rows = window.findChild<QTreeWidget *>("scanCandidates");
    QVERIFY(rows);
    QTRY_VERIFY_WITH_TIMEOUT(rows->topLevelItemCount() >= 3, 10000);
    QTRY_VERIFY(
        window.findChild<QPushButton *>("recoverySettings")->isEnabled());
    rows->setCurrentItem(rows->topLevelItem(0));
    window.findChild<QPushButton *>("previewCandidate")->click();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel *>("previewStatus")
                                 ->text()
                                 .contains("Nothing saved"),
                             10000);
    QTRY_VERIFY(
        window.findChild<QPushButton *>("recoverySettings")->isEnabled());
    window.saveScanAs(root.filePath("gui.rscan"));
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(root.filePath("gui.rscan")), 10000);
    QTRY_VERIFY(
        window.findChild<QPushButton *>("recoverySettings")->isEnabled());
    window.openSavedScan(root.filePath("gui.rscan"));
    QTRY_VERIFY(
        window.findChild<QPushButton *>("recoverySettings")->isEnabled());
    QTRY_VERIFY(rows->topLevelItemCount() >= 3);
    snapshot(window, "02-review-candidates");
    QVERIFY(QDir(root.filePath("out"))
                .entryList(QDir::AllEntries | QDir::NoDotAndDotDot)
                .isEmpty());
    for (int i = 0; i < rows->topLevelItemCount(); ++i)
      if (rows->topLevelItem(i)->data(0, Qt::UserRole).toString() ==
          "FAT12_32/Existing/HELLO.TXT")
        rows->topLevelItem(i)->setCheckState(0, Qt::Checked);
    window.findChild<QLineEdit *>("reviewOutput")
        ->setText(root.filePath("out"));
    window.findChild<QPushButton *>("recoverSelected")->click();
    QTRY_COMPARE_WITH_TIMEOUT(
        window.findChild<QLabel *>("resultStatus")->text(),
        QString("Completed"), 10000);
    QCOMPARE(read(root.filePath("out/FAT12_32/Existing/HELLO.TXT")),
             QByteArray("hello"));
    snapshot(window, "03-recovered-selection");
    QTRY_VERIFY(
        window.findChild<QPushButton *>("recoverySettings")->isEnabled());
    window.close();
  }
};
QTEST_MAIN(TestDiskRecovery)
#include "test_disk_recovery.moc"
