#include "Application/Service/GUIRecoveryService.h"
#include "GUI/core/MainWindow.h"
#include "GUI/widgets/SourceDiscoveryWidget.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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
