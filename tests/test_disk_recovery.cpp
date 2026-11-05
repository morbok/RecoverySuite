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
    if (qEnvironmentVariable("CI") != "true" || workspace.isEmpty()) return;
    const auto directory = workspace + "/build/screenshots";
    if (!QDir().mkpath(directory) || !widget.grab().save(directory + "/" + name + ".png"))
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
      if (rows->topLevelItem(i)->text(0) == "FAT12_32/Existing/HELLO.TXT")
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
