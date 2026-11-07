#include "Application/Service/GUIRecoveryService.h"
#include "Application/Service/RecoveryService.h"
#include "GUI/core/MainWindow.h"
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
using namespace recoverysuite;
class TestFatFiles : public QObject {
  Q_OBJECT
  static void le(QByteArray &data, int pos, uint64_t value, int width = 2) {
    for (int i = 0; i < width; ++i)
      data[pos + i] = char((value >> (i * 8)) & 255);
  }
  static void save(const QString &path, const QByteArray &data) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size())
      throw std::runtime_error("Fixture write failed");
  }
  static QByteArray load(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
      throw std::runtime_error("Fixture read failed");
    return file.readAll();
  }
  struct Volume {
    int ss, sectors, fatSectors, rootStart, dataStart;
    bool fat16;
    QByteArray data;
    Volume(bool sixteen = false, int sectorSize = 512)
        : ss(sectorSize), sectors(sixteen ? 5000 : 2880),
          fatSectors(sixteen ? 3 : 9), rootStart(1 + fatSectors * 2),
          dataStart(rootStart + (32 * 32 + ss - 1) / ss), fat16(sixteen),
          data(sectors * ss, char{0}) {
      le(data, 11, ss);
      data[13] = 1;
      le(data, 14, 1);
      data[16] = 2;
      le(data, 17, 32);
      le(data, 19, sectors);
      data[21] = char(0xf8);
      le(data, 22, fatSectors);
      data[510] = char(0x55);
      data[511] = char(0xaa);
      entry(0, sixteen ? 0xfff8 : 0xff8);
      entry(1, sixteen ? 0xffff : 0xfff);
      entry(2, 7);
      entry(7, 3);
      entry(3, sixteen ? 0xffff : 0xfff);
      directory(0, "FRAG    TXT", 2, 2 * ss + 17);
      directory(1, "EMPTY   TXT", 0, 0);
      directory(2, "DELETED TXT", 0, 0);
      data[rootStart * ss + 2 * 32] = char(0xe5);
      directory(3, "SUBDIR     ", 0, 0);
      data[rootStart * ss + 3 * 32 + 11] = char(0x10);
      data[(dataStart + 0) * ss] = 'A';
      data.replace((dataStart + 0) * ss, ss, QByteArray(ss, 'A'));
      data.replace((dataStart + 5) * ss, ss, QByteArray(ss, 'B'));
      data.replace((dataStart + 1) * ss, ss, QByteArray(ss, 'C'));
    }
    void entry(int cluster, unsigned value) {
      for (int copy = 0; copy < 2; ++copy) {
        const int base = (1 + copy * fatSectors) * ss;
        if (fat16)
          le(data, base + cluster * 2, value);
        else {
          const int pos = base + cluster + cluster / 2;
          unsigned pair =
              static_cast<unsigned char>(data[pos]) |
              (unsigned(static_cast<unsigned char>(data[pos + 1])) << 8);
          pair = (cluster & 1) ? ((pair & 0xf) | (value << 4))
                               : ((pair & 0xf000) | value);
          le(data, pos, pair);
        }
      }
    }
    void directory(int index, const char *name, unsigned first, unsigned size) {
      const int pos = rootStart * ss + index * 32;
      data.replace(pos, 11, QByteArray(name, 11));
      data[pos + 11] = char(0x20);
      le(data, pos + 26, first);
      le(data, pos + 28, size, 4);
    }
    QByteArray expected() const {
      return QByteArray(ss, 'A') + QByteArray(ss, 'B') + QByteArray(17, 'C');
    }
  };
  struct Fat32Volume {
    static constexpr int ss = 512, sectors = 70000, fatSectors = 600,
                         dataStart = 32 + 2 * fatSectors;
    QByteArray data = QByteArray(sectors * ss, char{0});
    Fat32Volume() {
      le(data, 11, ss);
      data[13] = 1;
      le(data, 14, 32);
      data[16] = 2;
      data[21] = char(0xf8);
      le(data, 32, sectors, 4);
      le(data, 36, fatSectors, 4);
      le(data, 44, 2, 4);
      data[510] = char(0x55);
      data[511] = char(0xaa);
      entry(0, 0x0ffffff8);
      entry(1, 0x0fffffff);
      entry(2, 10);
      entry(10, 0x0fffffff);
      // A full first root cluster forces traversal to a noncontiguous second
      // cluster.
      for (int i = 0; i < 16; ++i)
        data[dataStart * ss + i * 32] = char(0xe5);
      entry(65538, 9);
      entry(9, 4);
      entry(4, 0x0fffffff);
      const int pos = (dataStart + 8) * ss;
      data.replace(pos, 11, "HIGH    BIN");
      data[pos + 11] = char(0x20);
      le(data, pos + 20, 1);
      le(data, pos + 26, 2);
      le(data, pos + 28, 2 * ss + 17, 4);
      data.replace((dataStart + 65536) * ss, ss, QByteArray(ss, 'X'));
      data.replace((dataStart + 7) * ss, ss, QByteArray(ss, 'Y'));
      data.replace((dataStart + 2) * ss, ss, QByteArray(ss, 'Z'));
    }
    void entry(int cluster, uint32_t value, int copy = -1) {
      for (int n = 0; n < 2; ++n)
        if (copy < 0 || n == copy)
          le(data, (32 + n * fatSectors) * ss + cluster * 4, value, 4);
    }
    QByteArray expected() const {
      return QByteArray(ss, 'X') + QByteArray(ss, 'Y') + QByteArray(17, 'Z');
    }
  };
  static int longEntries(QByteArray &directory, const QString &name,
                         const QByteArray &alias) {
    const int count = (name.size() + 12) / 13;
    unsigned checksum = 0;
    for (unsigned char c : alias)
      checksum = (((checksum & 1) ? 128 : 0) + (checksum >> 1) + c) & 255;
    constexpr int offsets[]{1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
    for (int order = count; order > 0; --order) {
      const int pos = (count - order) * 32;
      directory.replace(pos, 32, QByteArray(32, char(0xff)));
      directory[pos] = char(order | (order == count ? 0x40 : 0));
      directory[pos + 11] = char(0x0f);
      directory[pos + 12] = 0;
      directory[pos + 13] = char(checksum);
      le(directory, pos + 26, 0);
      for (int j = 0; j < 13; ++j) {
        const int index = (order - 1) * 13 + j;
        le(directory, pos + offsets[j],
           index < name.size() ? name.utf16()[index]
                               : (index == name.size() ? 0 : 0xffff));
      }
    }
    return count;
  }
  static application::service::models::RecoveryOperation
  operation(const std::string &path, const QString &output, uint64_t start,
            uint64_t count) {
    application::service::models::RecoveryOperation op{};
    op.operationType = "file_recovery";
    op.targetDevicePath = path;
    op.outputPath = output.toStdString();
    op.startSector = start;
    op.numSectors = count;
    op.verifyAfterRecovery = true;
    return op;
  }
private slots:
  void automaticLargeFatFilesUseBoundedStreaming() {
    for (bool crosslink : {false, true}) {
      QTemporaryDir root;
      Volume volume(true, 4096);
      volume.data.replace(volume.rootStart * volume.ss, volume.ss,
                          QByteArray(volume.ss, 0));
      const int size = 17 * 1024 * 1024 + 123,
                needed = (size + volume.ss - 1) / volume.ss,
                half = (needed + 1) / 2;
      const auto physical = [&](int logical) {
        return logical % 2 ? 2 + half + logical / 2 : 2 + logical / 2;
      };
      QByteArray payload;
      payload.reserve(size);
      for (int i = 0; i < needed; ++i) {
        const auto cluster = physical(i);
        volume.entry(cluster, i + 1 == needed ? 0xffff : physical(i + 1));
        const QByteArray block(volume.ss, char(i % 251));
        volume.data.replace((volume.dataStart + cluster - 2) * volume.ss,
                            volume.ss, block);
        payload.append(block.constData(),
                       std::min<qsizetype>(volume.ss, size - payload.size()));
      }
      volume.directory(0, "BIGDATA BIN", physical(0), size);
      if (crosslink)
        volume.directory(1, "ALIASED BIN", physical(0), size);
      const auto path = root.filePath("large-fat.img");
      save(path, volume.data);
      application::service::GUIRecoveryService service(nullptr);
      const auto device = service.openImage(path.toStdString(), 4096);
      auto op = operation(device.devicePath, {}, 0, volume.sectors);
      op.operationType = "disk_scan";
      op.deepSearch = false;
      const auto scanned = service.executeOperation(op);
      QVERIFY2(scanned.success, scanned.errorMessage.c_str());
      QCOMPARE(scanned.recoveredItemsCount, uint64_t(crosslink ? 0 : 1));
      QVERIFY(!scanned.outputWritten);
      if (!crosslink) {
        const std::string name = "FAT16_0/Existing/BIGDATA.BIN";
        QCOMPARE(scanned.recoveredItems[0].sizeBytes, uint64_t(size));
        QCOMPARE(service.previewCandidate(name).size(), size_t(65536));
        service.saveScan(root.filePath("large.rscan").toStdString());
        QVERIFY(service.loadScan(root.filePath("large.rscan").toStdString())
                    .success);
        QVERIFY(QDir().mkdir(root.filePath("out")));
        op.operationType = "disk_recovery";
        op.outputPath = root.filePath("out").toStdString();
        op.selectedPaths = {name};
        const auto recovered = service.executeOperation(op);
        QVERIFY2(recovered.success, recovered.errorMessage.c_str());
        QCOMPARE(load(root.filePath("out/FAT16_0/Existing/BIGDATA.BIN")),
                 payload);
      }
      QCOMPARE(
          QCryptographicHash::hash(load(path), QCryptographicHash::Sha256),
          QCryptographicHash::hash(volume.data, QCryptographicHash::Sha256));
    }
  }

  void copiesFragmentedAllocatedFiles_data() {
    QTest::addColumn<bool>("sixteen");
    QTest::addColumn<int>("ss");
    QTest::addColumn<bool>("dirty");
    QTest::newRow("FAT12") << false << 512 << false;
    QTest::newRow("FAT16-4k") << true << 4096 << false;
    QTest::newRow("FAT16-dirty-flags") << true << 4096 << true;
  }
  void copiesFragmentedAllocatedFiles() {
#ifndef _WIN32
    QFETCH(bool, sixteen);
    QFETCH(int, ss);
    QFETCH(bool, dirty);
    QTemporaryDir root;
    Volume volume(sixteen, ss);
    if (dirty)
      volume.entry(1, 0x3fff);
    const auto path = root.filePath("source.img");
    const auto image = QByteArray(3 * ss, char(0x7a)) + volume.data;
    save(path, image);
    application::service::GUIRecoveryService service(nullptr);
    const auto source = service.openImage(path.toStdString(), ss);
    const auto op =
        operation(source.devicePath, root.path(), 3, volume.sectors);
    const auto result = service.executeOperation(op);
    QVERIFY2(result.success, result.errorMessage.c_str());
    QCOMPARE(result.recoveredItemsCount, uint64_t{2});
    QCOMPARE(result.recoveredBytes, uint64_t(volume.expected().size()));
    QCOMPARE(load(root.filePath("FRAG.TXT")), volume.expected());
    QCOMPARE(load(root.filePath("EMPTY.TXT")), QByteArray());
    QCOMPARE(load(path), image);
    QCOMPARE(result.details.at("Deleted entries not reconstructed"),
             std::string("1"));
    QCOMPARE(result.details.at("Subdirectories not traversed"),
             std::string("1"));
    const auto repeated = service.executeOperation(op);
    QVERIFY(!repeated.success);
    QCOMPARE(repeated.failedItemsCount, uint64_t{2});
    QCOMPARE(load(root.filePath("FRAG.TXT")), volume.expected());
    auto reader = std::make_shared<disk::DiskImageReader>(ss);
    reader->open(path.toStdString());
    application::service::RecoveryService legacy(reader);
    const auto buffered = legacy.recoverFiles(3, volume.sectors);
    QVERIFY(buffered.success);
    QCOMPARE(buffered.recoveredFiles.size(), size_t{2});
    QCOMPARE(QByteArray(reinterpret_cast<const char *>(
                            buffered.recoveredFiles.at("FRAG.TXT").data()),
                        buffered.recoveredFiles.at("FRAG.TXT").size()),
             volume.expected());
#endif
  }
  void deletedCandidates_data() {
    QTest::addColumn<QString>("kind");
    for (const char *k : {"fat12", "fat16", "fat32", "nested", "allocated",
                          "allocated-tail", "live-cleared", "oversized-live",
                          "unsafe-live", "overlap-candidates"})
      QTest::newRow(k) << QString(k);
  }
  void deletedCandidates() {
#ifndef _WIN32
    QFETCH(QString, kind);
    QTemporaryDir root;
    Volume v(kind == "fat16", kind == "fat16" ? 4096 : 512);
    const int ss = v.ss;
    v.directory(2, "DELETED TXT", 20, ss + 17);
    v.data[v.rootStart * ss + 2 * 32] = char(0xe5);
    v.data[v.rootStart * ss + 3 * 32] = 0;
    v.data.replace((v.dataStart + 18) * ss, ss, QByteArray(ss, 'D'));
    v.data.replace((v.dataStart + 19) * ss, ss, QByteArray(ss, 'E'));
    if (kind == "allocated")
      v.entry(20, 0xfff);
    if (kind == "allocated-tail")
      v.entry(21, 0xfff);
    if (kind == "live-cleared")
      v.directory(0, "LIVE    BIN", 20, 2 * ss);
    if (kind == "oversized-live")
      v.directory(0, "LIVE    BIN", 20, 17 * 1024 * 1024);
    if (kind == "unsafe-live")
      v.directory(0, "BAD/FILETXT", 20, 2 * ss);
    if (kind == "overlap-candidates") {
      v.directory(3, "SECOND  TXT", 20, ss + 17);
      v.data[v.rootStart * ss + 3 * 32] = char(0xe5);
      v.data[v.rootStart * ss + 4 * 32] = 0;
    }
    QString name = "deleted-0-2-_ELETED.TXT";
    if (kind == "nested") {
      v.directory(2, "VOLUME     ", 0, 0);
      v.data[v.rootStart * ss + 2 * 32 + 11] = 8;
      v.directory(3, "SUBDIR     ", 30, 0);
      v.data[v.rootStart * ss + 3 * 32 + 11] = 0x10;
      v.entry(30, 0xfff);
      const int pos = (v.dataStart + 28) * ss;
      v.data.replace(pos, 11, "DELETED TXT");
      v.data[pos] = char(0xe5);
      v.data[pos + 11] = 0x20;
      le(v.data, pos + 26, 20);
      le(v.data, pos + 28, ss + 17, 4);
      name = "SUBDIR/deleted-1-0-_ELETED.TXT";
    }
    QByteArray bytes = v.data;
    int sectors = v.sectors;
    if (kind == "fat32") {
      Fat32Volume f;
      const int pos = f.dataStart * 512;
      f.data.replace(pos, 32, QByteArray(32, char{0}));
      f.data.replace(pos, 11, "DELETED TXT");
      f.data[pos] = char(0xe5);
      f.data[pos + 11] = 0x20;
      le(f.data, pos + 26, 1000);
      le(f.data, pos + 28, 512 + 17, 4);
      f.data[pos + 32] = 0;
      f.data.replace((f.dataStart + 998) * 512, 512, QByteArray(512, 'D'));
      f.data.replace((f.dataStart + 999) * 512, 512, QByteArray(512, 'E'));
      bytes = f.data;
      sectors = f.sectors;
      name = "deleted-0-0-_ELETED.TXT";
    }
    const auto path = root.filePath("source.img");
    const auto original = QByteArray(3 * ss, char(0x33)) + bytes;
    save(path, original);
    application::service::GUIRecoveryService service(nullptr);
    const auto source = service.openImage(path.toStdString(), ss);
    auto op = operation(source.devicePath, root.path(), 3, sectors);
    op.operationType = "fat_deleted_recovery";
    const auto result = service.executeOperation(op);
    const bool rejected = kind == "allocated" || kind == "allocated-tail" ||
                          kind == "live-cleared" || kind == "oversized-live" ||
                          kind == "unsafe-live";
    QCOMPARE(result.recoveredItemsCount, uint64_t(rejected ? 0 : 1));
    QCOMPARE(result.success, !rejected && kind != "overlap-candidates");
    QCOMPARE(load(path), original);
    if (!rejected) {
      const auto expected = QByteArray(ss, 'D') + QByteArray(17, 'E');
      QCOMPARE(load(root.filePath(name)), expected);
      QVERIFY(result.details.at("Recovery confidence").find("Low") !=
              std::string::npos);
      const auto out = root.filePath("cli");
      QVERIFY(QDir().mkdir(out));
      QProcess cli;
      cli.start(QCoreApplication::applicationDirPath() + "/RecoverySuite_CLI",
                {"image", "fat-deleted", path, "--start", "3", "--sector-size",
                 QString::number(ss), "--output", out});
      QVERIFY(cli.waitForFinished(5000));
      QCOMPARE(cli.exitCode(), kind == "overlap-candidates" ? 1 : 0);
      QCOMPARE(load(out + "/" + name), expected);
      const auto repeated = service.executeOperation(op);
      QVERIFY(!repeated.success);
      QCOMPARE(load(root.filePath(name)), expected);
      if (kind == "fat12") {
        const auto guiOut = root.filePath("gui");
        QVERIFY(QDir().mkdir(guiOut));
        gui::core::MainWindow window(&service);
        window.show();
        window.findChild<QLineEdit *>("imagePath")->setText(path);
        window.findChild<QPushButton *>("openImage")->click();
        window.findChild<QComboBox *>("recoveryMethod")->setCurrentIndex(9);
        window.findChild<QLineEdit *>("recoveryStart")->setText("3");
        window.findChild<QLineEdit *>("recoveryCount")
            ->setText(QString::number(sectors));
        window.findChild<QLineEdit *>("recoveryOutput")->setText(guiOut);
        window.findChild<QPushButton *>("startImageRecovery")->click();
        QTRY_COMPARE_WITH_TIMEOUT(
            window.findChild<QLabel *>("resultStatus")->text(),
            QString("Completed"), 5000);
        QTRY_VERIFY(
            window.findChild<QPushButton *>("recoverySettings")->isEnabled());
        window.close();
        QCOMPARE(load(guiOut + "/" + name), expected);
        const auto cancelOut = root.filePath("cancel");
        QVERIFY(QDir().mkdir(cancelOut));
        op.outputPath = cancelOut.toStdString();
        bool stop = false;
        const auto stopped = service.executeOperation(
            op,
            [&](const auto &p) {
              if (p.processedBytes > uint64_t((v.dataStart) * ss))
                stop = true;
            },
            [&] { return stop; });
        QVERIFY(stopped.cancelled);
        QVERIFY(!QFile::exists(cancelOut + "/" + name));
      }

    } else
      QVERIFY(!QFile::exists(root.filePath(name)));
#endif
  }
  void invalidChainsNamesAndMirrors_data() {
    QTest::addColumn<QString>("fault");
    for (const char *value :
         {"cycle", "short", "long", "free", "bad", "outside", "name", "size",
          "crosslink", "directory-first", "directory-tail", "mirrors", "range"})
      QTest::newRow(value) << QString(value);
  }
  void invalidChainsNamesAndMirrors() {
#ifndef _WIN32
    QFETCH(QString, fault);
    QTemporaryDir root;
    Volume volume;
    uint64_t count = volume.sectors;
    if (fault == "cycle")
      volume.entry(7, 2);
    if (fault == "short")
      volume.entry(2, 0xfff);
    if (fault == "long")
      volume.entry(3, 4);
    if (fault == "free")
      volume.entry(7, 0);
    if (fault == "bad")
      volume.entry(7, 0xff7);
    if (fault == "outside")
      volume.entry(7, 4000);
    if (fault == "name")
      volume.directory(0, "../EVIL TXT", 2, 2 * volume.ss + 17);
    if (fault == "size")
      volume.directory(0, "FRAG    TXT", 2, 17 * 1024 * 1024);
    if (fault == "crosslink")
      volume.directory(2, "OTHER   TXT", 2, 2 * volume.ss + 17);
    if (fault == "directory-first" || fault == "directory-tail") {
      volume.directory(3, "SUBDIR     ", fault == "directory-first" ? 2 : 5, 0);
      volume.data[volume.rootStart * volume.ss + 3 * 32 + 11] = char(0x10);
      if (fault == "directory-tail")
        volume.entry(5, 7);
    }
    if (fault == "mirrors")
      volume.data[(1 + volume.fatSectors) * volume.ss + 5] ^= 1;
    if (fault == "range")
      count = volume.sectors - 1;
    const auto path = root.filePath("source.img");
    save(path, volume.data);
    application::service::GUIRecoveryService service(nullptr);
    const auto source = service.openImage(path.toStdString());
    const auto result = service.executeOperation(
        operation(source.devicePath, root.path(), 0, count));
    QVERIFY(!result.success);
    if (fault == "mirrors" || fault == "range") {
      QVERIFY(!result.outputWritten);
      QVERIFY(!QFile::exists(root.filePath("EMPTY.TXT")));
    } else {
      QVERIFY(QFile::exists(root.filePath("EMPTY.TXT")));
      QCOMPARE(result.details.at("Invalid or unsupported root files"),
               std::string("1"));
    }
    if (fault != "crosslink")
      QVERIFY(!QFile::exists(root.filePath("FRAG.TXT")));
#endif
  }
  void longNames_data() {
    QTest::addColumn<QString>("variant");
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("kind");
    QTest::newRow("unicode-FAT12")
        << QString("valid") << QString::fromUtf8("Kurtarılan fotoğraf 📷.bin")
        << 12;
    QTest::newRow("surrogate-across-records-FAT16")
        << QString("valid") << QString::fromUtf8("abcdefghijkl📷.bin") << 16;
    QTest::newRow("across-root-clusters-FAT32")
        << QString("valid") << QString("Long fragmented root filename.bin")
        << 32;
    QTest::newRow("exact-thirteen")
        << QString("valid") << QString("123456789.bin") << 12;
    QTest::newRow("maximum-name")
        << QString("valid") << QString(251, 'a').append(".bin") << 12;
    for (const char *fault :
         {"checksum", "ordinal", "missing-last", "type", "cluster", "padding",
          "surrogate", "reserved-order", "deleted", "interrupted"})
      QTest::newRow(fault) << QString(fault)
                           << QString("A longer restored filename.bin") << 12;
    for (const char *unsafe :
         {"../evil.bin", "folder/file.bin", "drive:file.bin", "trailing.",
          "trailing ", ".", ".."})
      QTest::newRow(unsafe) << QString("unsafe") << QString(unsafe) << 12;
    QTest::newRow("utf8-byte-limit")
        << QString("unsafe") << QString(100, QChar(0x4e2d)) << 12;
  }
  void longNames() {
#ifndef _WIN32
    QFETCH(QString, variant);
    QFETCH(QString, name);
    QFETCH(int, kind);
    QTemporaryDir root;
    Volume fixed(kind == 16, kind == 16 ? 4096 : 512);
    Fat32Volume large;
    QByteArray directory(1024, char{0});
    const QByteArray alias =
        kind == 32 ? QByteArray("HIGH    BIN") : QByteArray("FRAG    TXT");
    const int records = longEntries(directory, name, alias),
              shortPos = records * 32;
    const int original =
        kind == 32 ? (large.dataStart + 8) * 512 : fixed.rootStart * fixed.ss;
    directory.replace(shortPos, 32,
                      (kind == 32 ? large.data : fixed.data).mid(original, 32));
    if (variant == "checksum")
      directory[13] ^= 1;
    if (variant == "ordinal")
      directory[32] = char(3);
    if (variant == "missing-last")
      directory[0] = char(static_cast<unsigned char>(directory[0]) & ~0x40);
    if (variant == "type")
      directory[12] = 1;
    if (variant == "cluster")
      le(directory, 26, 1);
    if (variant == "padding")
      le(directory, 30, 'x');
    if (variant == "surrogate")
      le(directory, (records - 1) * 32 + 1, 0xd800);
    if (variant == "reserved-order")
      directory[0] = char(static_cast<unsigned char>(directory[0]) | 0x20);
    if (variant == "deleted")
      directory[32] = char(0xe5);
    if (variant == "interrupted")
      directory[32 + 11] = char(8);
    QByteArray image;
    int sectors = 0;
    QString aliasName;
    if (kind == 32) {
      // Split the LFN sequence itself across two noncontiguous root clusters.
      QByteArray logical(1024, char{0});
      for (int i = 0; i < 15; ++i)
        logical[i * 32] = char(0xe5);
      logical.replace(15 * 32, directory.size() - 15 * 32,
                      directory.left(directory.size() - 15 * 32));
      large.data.replace(large.dataStart * 512, 512, logical.left(512));
      large.data.replace((large.dataStart + 8) * 512, 512, logical.mid(512));
      image = large.data;
      sectors = large.sectors;
      aliasName = "HIGH.BIN";
    } else {
      fixed.data.replace(fixed.rootStart * fixed.ss, 1024, directory);
      image = fixed.data;
      sectors = fixed.sectors;
      aliasName = "FRAG.TXT";
    }
    const auto path = root.filePath("source.img");
    save(path, image);
    application::service::GUIRecoveryService service(nullptr);
    const auto source =
        service.openImage(path.toStdString(), kind == 16 ? 4096 : 512);
    const auto result = service.executeOperation(
        operation(source.devicePath, root.path(), 0, sectors));
    QVERIFY2(result.success, result.errorMessage.c_str());
    QCOMPARE(result.recoveredItemsCount, uint64_t{1});
    const auto outputName = variant == "valid" ? name : aliasName;
    QCOMPARE(load(root.filePath(outputName)),
             kind == 32 ? large.expected() : fixed.expected());
    if (variant != "valid" && name != aliasName && name != "." &&
        name != ".." && name.toUtf8().size() <= 255)
      QVERIFY(!QFile::exists(root.filePath(name)));
    QCOMPARE(load(path), image);
    if (variant == "valid")
      QCOMPARE(result.details.at("Long names rejected (8.3 fallback)"),
               std::string("0"));
    if (kind == 32) {
      const auto output = root.filePath("cli");
      QVERIFY(QDir().mkdir(output));
      QProcess cli;
      cli.start(QCoreApplication::applicationDirPath() + "/RecoverySuite_CLI",
                {"image", "files", path, "--output", output});
      QVERIFY(cli.waitForFinished(5000));
      QCOMPARE(cli.exitCode(), 0);
      QCOMPARE(load(QDir(output).filePath(name)), large.expected());
    }
    if (variant == "checksum" || variant == "unsafe")
      QCOMPARE(result.details.at("Long names rejected (8.3 fallback)"),
               std::string("1"));
#endif
  }
  void fat32RootChains_data() {
    QTest::addColumn<QString>("variant");
    for (const char *value :
         {"mirrored", "active-second", "reserved-high-bits", "dirty-flags",
          "root-cycle", "root-bad", "root-empty", "root-crosslink",
          "file-cycle", "mirror-mismatch", "active-outside", "version",
          "reserved-flags", "cancel"})
      QTest::newRow(value) << QString(value);
  }
  void fat32RootChains() {
#ifndef _WIN32
    QFETCH(QString, variant);
    QTemporaryDir root;
    Fat32Volume volume;
    if (variant == "active-second") {
      le(volume.data, 40, 0x81);
      volume.entry(65538, 0, 0);
      volume.entry(2, 0, 0);
    }
    if (variant == "reserved-high-bits") {
      volume.entry(65538, 0xa0000009, 0);
      volume.entry(10, 0xffffffff);
    }
    if (variant == "dirty-flags")
      volume.entry(1, 0x03ffffff);
    if (variant == "root-cycle")
      volume.entry(10, 2);
    if (variant == "root-bad")
      volume.entry(2, 0x0ffffff7);
    if (variant == "root-empty")
      le(volume.data, 44, 0x0fffffff, 4);
    if (variant == "root-crosslink") {
      const int pos = (volume.dataStart + 8) * 512;
      le(volume.data, pos + 20, 0);
      le(volume.data, pos + 26, 10);
      le(volume.data, pos + 28, 512, 4);
    }
    if (variant == "file-cycle")
      volume.entry(9, 65538);
    if (variant == "mirror-mismatch")
      volume.entry(65538, 0, 0);
    if (variant == "active-outside")
      le(volume.data, 40, 0x82);
    if (variant == "version")
      le(volume.data, 42, 1);
    if (variant == "reserved-flags")
      le(volume.data, 40, 0x100);
    const auto path = root.filePath("source.img");
    save(path, volume.data);
    application::service::GUIRecoveryService service(nullptr);
    const auto source = service.openImage(path.toStdString());
    bool cancel = false;
    const auto result = service.executeOperation(
        operation(source.devicePath, root.path(), 0, volume.sectors),
        [&](const auto &progress) {
          if (variant == "cancel" &&
              progress.processedBytes >= 512 + 2 * 600 * 512 + 512)
            cancel = true;
        },
        [&] { return cancel; });
    const bool valid = variant == "mirrored" || variant == "active-second" ||
                       variant == "reserved-high-bits" ||
                       variant == "dirty-flags";
    QCOMPARE(result.success, valid);
    QCOMPARE(result.outputWritten, valid);
    if (valid) {
      QCOMPARE(result.recoveredItemsCount, uint64_t{1});
      QCOMPARE(load(root.filePath("HIGH.BIN")), volume.expected());
      QCOMPARE(result.details.at("Deleted entries not reconstructed"),
               std::string("16"));
    } else
      QVERIFY(!QFile::exists(root.filePath("HIGH.BIN")));
    if (variant == "cancel")
      QVERIFY(result.cancelled);
    QCOMPARE(load(path), volume.data);
    if (variant == "active-second") {
      const auto output = root.filePath("cli");
      QVERIFY(QDir().mkdir(output));
      QProcess cli;
      cli.start(QCoreApplication::applicationDirPath() + "/RecoverySuite_CLI",
                {"image", "files", path, "--output", output});
      QVERIFY(cli.waitForFinished(5000));
      QCOMPARE(cli.exitCode(), 0);
      QCOMPARE(load(output + "/HIGH.BIN"), volume.expected());
    }
#endif
  }
  void recursiveFiles_data() {
    QTest::addColumn<int>("kind");
    for (int kind : {12, 16, 32})
      QTest::newRow(qPrintable(QString::number(kind))) << kind;
  }
  void recursiveFiles() {
#ifndef _WIN32
    QFETCH(int, kind);
    QTemporaryDir root;
    Volume fixed(kind == 16, kind == 16 ? 4096 : 512);
    Fat32Volume large;
    auto &image = kind == 32 ? large.data : fixed.data;
    const int ss = kind == 16 ? 4096 : 512,
              dataStart = kind == 32 ? large.dataStart : fixed.dataStart;
    const int sectors = kind == 32 ? large.sectors : fixed.sectors,
              rootPos = kind == 32 ? (dataStart + 8) * ss + 32
                                   : fixed.rootStart * ss + 3 * 32;
    const auto entry = [&](int cluster, unsigned value) {
      if (kind == 32)
        large.entry(cluster, value);
      else
        fixed.entry(cluster, value);
    };
    const unsigned end =
        kind == 32 ? 0x0fffffff : (kind == 16 ? 0xffff : 0xfff);
    const auto record = [&](QByteArray &bytes, int pos, const QByteArray &name,
                            int attr, unsigned cluster, unsigned size) {
      bytes.replace(pos, 32, QByteArray(32, char{0}));
      bytes.replace(pos, 11, name);
      bytes[pos + 11] = char(attr);
      if (kind == 32)
        le(bytes, pos + 20, cluster >> 16);
      le(bytes, pos + 26, cluster & 65535);
      le(bytes, pos + 28, size, 4);
    };
    entry(5, 8);
    entry(8, end);
    entry(6, end);
    entry(11, 14);
    entry(14, end);
    entry(12, end);
    record(image, rootPos, "SUBDIR     ", 16, 5, 0);
    QByteArray directory(2 * ss, char{0});
    for (int i = 0; i < ss / 32; ++i)
      directory[i * 32] = char(0xe5);
    record(directory, 0, ".          ", 16, 5, 0);
    record(directory, 32, "..         ", 16, 0, 0);
    const auto longName = QString::fromUtf8("İç klasör verisi 📷.bin");
    QByteArray longRecords(256, char{0});
    const int names = longEntries(longRecords, longName, "CHILD   BIN");
    const int nameStart = ss - 32;
    directory.replace(nameStart, names * 32, longRecords.left(names * 32));
    const int filePos = nameStart + names * 32;
    record(directory, filePos, "CHILD   BIN", 32, 11, ss + 7);
    record(directory, filePos + 32, "NESTED     ", 16, 6, 0);
    record(directory, filePos + 64, "EMPTY   TXT", 32, 0, 0);
    image.replace((dataStart + 3) * ss, ss, directory.left(ss));
    image.replace((dataStart + 6) * ss, ss, directory.mid(ss));
    QByteArray nested(ss, char{0});
    record(nested, 0, ".          ", 16, 6, 0);
    record(nested, 32, "..         ", 16, 5, 0);
    record(nested, 64, "INNER   TXT", 32, 12, 5);
    image.replace((dataStart + 4) * ss, ss, nested);
    image.replace((dataStart + 9) * ss, ss, QByteArray(ss, 'P'));
    image.replace((dataStart + 12) * ss, ss, QByteArray(ss, 'Q'));
    image.replace((dataStart + 10) * ss, 5, "hello");
    const auto path = root.filePath("source.img");
    const auto original = QByteArray(3 * ss, char(0x66)) + image;
    save(path, original);
    const auto output = root.filePath("out");
    QVERIFY(QDir().mkdir(output));
    application::service::GUIRecoveryService service(nullptr);
    const auto source = service.openImage(path.toStdString(), ss);
    auto op = operation(source.devicePath, output, 3, sectors);
    op.operationType = "file_tree_recovery";
    const auto result = service.executeOperation(op);
    QVERIFY2(result.success, result.errorMessage.c_str());
    QCOMPARE(result.recoveredItemsCount, uint64_t(kind == 32 ? 4 : 5));
    QCOMPARE(result.details.at("Directories traversed"), std::string("2"));
    QCOMPARE(load(QDir(output).filePath("SUBDIR/" + longName)),
             QByteArray(ss, 'P') + QByteArray(7, 'Q'));
    QCOMPARE(load(output + "/SUBDIR/NESTED/INNER.TXT"), QByteArray("hello"));
    QCOMPARE(load(output + "/SUBDIR/EMPTY.TXT"), QByteArray());
    const auto repeated = service.executeOperation(op);
    QVERIFY(!repeated.success);
    QCOMPARE(repeated.failedItemsCount, result.recoveredItemsCount);
    QCOMPARE(load(path), original);
    if (kind == 12) {
      const auto guiOut = root.filePath("gui");
      QVERIFY(QDir().mkdir(guiOut));
      gui::core::MainWindow window(&service);
      window.show();
      window.findChild<QLineEdit *>("imagePath")->setText(path);
      window.findChild<QPushButton *>("openImage")->click();
      window.findChild<QComboBox *>("recoveryMethod")->setCurrentIndex(3);
      window.findChild<QLineEdit *>("recoveryStart")->setText("3");
      window.findChild<QLineEdit *>("recoveryCount")
          ->setText(QString::number(sectors));
      window.findChild<QLineEdit *>("recoveryOutput")->setText(guiOut);
      window.findChild<QPushButton *>("startImageRecovery")->click();
      QTRY_COMPARE_WITH_TIMEOUT(
          window.findChild<QLabel *>("resultStatus")->text(),
          QString("Completed"), 5000);
      QTRY_VERIFY(
          window.findChild<QPushButton *>("recoverySettings")->isEnabled());
      window.close();
      QCOMPARE(load(guiOut + "/SUBDIR/NESTED/INNER.TXT"), QByteArray("hello"));
    }
    if (kind == 32) {
      const auto cliOut = root.filePath("cli");
      QVERIFY(QDir().mkdir(cliOut));
      QProcess cli;
      cli.start(QCoreApplication::applicationDirPath() + "/RecoverySuite_CLI",
                {"image", "tree", path, "--start", "3", "--count",
                 QString::number(sectors), "--output", cliOut});
      QVERIFY(cli.waitForFinished(5000));
      QCOMPARE(cli.exitCode(), 0);
      QCOMPARE(load(cliOut + "/SUBDIR/" + longName),
               QByteArray(ss, 'P') + QByteArray(7, 'Q'));
    }
    const auto cancelOut = root.filePath("cancel");
    QVERIFY(QDir().mkdir(cancelOut));
    op.outputPath = cancelOut.toStdString();
    bool cancel = false;
    const uint64_t metadata = kind == 32 ? 512 + 2 * 600 * 512 + 2 * 512
                                         : ss + 2 * fixed.fatSectors * ss +
                                               ((32 * 32 + ss - 1) / ss) * ss;
    const auto stopped = service.executeOperation(
        op,
        [&](const auto &progress) {
          if (progress.processedBytes > metadata)
            cancel = true;
        },
        [&] { return cancel; });
    QVERIFY(stopped.cancelled);
    QVERIFY(!stopped.outputWritten);
    QVERIFY(QDir(cancelOut)
                .entryList(QDir::NoDotAndDotDot | QDir::AllEntries)
                .isEmpty());
#endif
  }
  void recursiveDirectoryBudgets_data() {
    QTest::addColumn<bool>("bytes");
    QTest::newRow("directory-attempt-limit") << false;
    QTest::newRow("directory-byte-limit") << true;
  }
  void recursiveDirectoryBudgets() {
#ifndef _WIN32
    QFETCH(bool, bytes);
    QTemporaryDir root;
    QByteArray image;
    uint64_t count = 0;
    if (bytes) {
      Fat32Volume volume;
      const int pos = (volume.dataStart + 8) * 512 + 32;
      volume.data.replace(pos, 11, "HUGE       ");
      volume.data[pos + 11] = 16;
      le(volume.data, pos + 26, 100);
      for (int c = 100; c < 32869; ++c)
        volume.entry(c, c == 32868 ? 0x0fffffff : c + 1);
      image = volume.data;
      count = volume.sectors;
    } else {
      Volume volume;
      volume.directory(3, "MANY       ", 5, 0);
      volume.data[volume.rootStart * 512 + 3 * 32 + 11] = 16;
      volume.entry(5, 20);
      for (int c = 20; c <= 83; ++c)
        volume.entry(c, c == 83 ? 0xfff : c + 1);
      int index = 0;
      for (int c = 5; c <= 83; c = (c == 5 ? 20 : c + 1))
        for (int n = 0; n < 16; ++n) {
          const int pos = (volume.dataStart + c - 2) * 512 + n * 32;
          volume.data.replace(
              pos, 11,
              QString("D%1   ").arg(index++, 7, 10, QChar('0')).toLatin1());
          volume.data[pos + 11] = 16;
        }
      image = volume.data;
      count = volume.sectors;
    }
    const auto path = root.filePath("source.img");
    save(path, image);
    application::service::GUIRecoveryService service(nullptr);
    const auto source = service.openImage(path.toStdString());
    const auto output = root.filePath("out");
    QVERIFY(QDir().mkdir(output));
    auto op = operation(source.devicePath, output, 0, count);
    op.operationType = "file_tree_recovery";
    const auto result = service.executeOperation(op);
    QVERIFY(!result.success);
    QCOMPARE(load(path), image);
    if (bytes) {
      QCOMPARE(result.details.at("Invalid or unsupported directories"),
               std::string("1"));
      QCOMPARE(load(output + "/HIGH.BIN"), Fat32Volume().expected());
    } else {
      QVERIFY(!result.outputWritten);
      QVERIFY(QString::fromStdString(result.errorMessage)
                  .contains("directory item limit"));
    }
#endif
  }
  void recursiveCorruption_data() {
    QTest::addColumn<QString>("fault");
    for (const char *f :
         {"directory-cycle", "file-cycle", "parent-cycle", "directory-data",
          "invalid-directory-name", "depth", "symlink"})
      QTest::newRow(f) << QString(f);
  }
  void recursiveCorruption() {
#ifndef _WIN32
    QFETCH(QString, fault);
    QTemporaryDir root;
    Volume volume;
    volume.entry(5, 0xfff);
    volume.directory(3, "SUBDIR     ", 5, 0);
    volume.data[volume.rootStart * 512 + 3 * 32 + 11] = 16;
    const int child = (volume.dataStart + 3) * 512;
    volume.data.replace(child, 11, "CHILD   TXT");
    volume.data[child + 11] = 32;
    le(volume.data, child + 26, 6);
    le(volume.data, child + 28, 4, 4);
    volume.entry(6, 0xfff);
    volume.data.replace((volume.dataStart + 4) * 512, 4, "test");
    if (fault == "directory-cycle")
      volume.entry(5, 5);
    if (fault == "file-cycle") {
      volume.entry(6, 6);
      le(volume.data, child + 28, 513, 4);
    }
    if (fault == "parent-cycle") {
      volume.data[child + 11] = 16;
      le(volume.data, child + 26, 5);
      le(volume.data, child + 28, 0, 4);
    }
    if (fault == "directory-data")
      le(volume.data, child + 26, 5);
    if (fault == "invalid-directory-name") {
      volume.directory(3, "../BAD     ", 2, 0);
      volume.data[volume.rootStart * 512 + 3 * 32 + 11] = 16;
    }
    if (fault == "depth") {
      volume.data[child + 11] = 16;
      le(volume.data, child + 26, 20);
      le(volume.data, child + 28, 0, 4);
      for (int i = 20; i < 55; ++i) {
        volume.entry(i, 0xfff);
        const int pos = (volume.dataStart + i - 2) * 512;
        volume.data.replace(pos, 11, "NEXT       ");
        volume.data[pos + 11] = 16;
        le(volume.data, pos + 26, i + 1);
      }
    }
    const auto path = root.filePath("source.img");
    save(path, volume.data);
    const auto output = root.filePath("out");
    QVERIFY(QDir().mkdir(output));
    const auto outside = root.filePath("outside");
    QVERIFY(QDir().mkdir(outside));
    if (fault == "symlink")
      QVERIFY(QFile::link(outside, output + "/SUBDIR"));
    application::service::GUIRecoveryService service(nullptr);
    const auto source = service.openImage(path.toStdString());
    auto op = operation(source.devicePath, output, 0, volume.sectors);
    op.operationType = "file_tree_recovery";
    const auto result = service.executeOperation(op);
    QVERIFY(!result.success);
    QVERIFY(result.outputWritten);
    if (fault == "invalid-directory-name") {
      QVERIFY(!QFile::exists(output + "/FRAG.TXT"));
      QVERIFY(QFile::exists(output + "/EMPTY.TXT"));
    } else
      QCOMPARE(load(output + "/FRAG.TXT"), volume.expected());
    QVERIFY(!QFile::exists(output + "/SUBDIR/CHILD.TXT"));
    QCOMPARE(load(path), volume.data);
    QVERIFY(QDir(outside)
                .entryList(QDir::NoDotAndDotDot | QDir::AllEntries)
                .isEmpty());
    if (fault == "directory-cycle" || fault == "parent-cycle" ||
        fault == "depth")
      QCOMPARE(result.details.at("Invalid or unsupported directories"),
               std::string("1"));
    if (fault == "directory-data" || fault == "file-cycle")
      QCOMPARE(result.details.at("Invalid or unsupported files"),
               std::string("1"));
#endif
  }
  void guiCliAndCancellation() {
#ifndef _WIN32
    QTemporaryDir root;
    Volume volume;
    const auto path = root.filePath("source.img");
    save(path, volume.data);
    const auto guiOut = root.filePath("gui"), cliOut = root.filePath("cli");
    QVERIFY(QDir().mkdir(guiOut));
    QVERIFY(QDir().mkdir(cliOut));
    application::service::GUIRecoveryService service(nullptr);
    gui::core::MainWindow window(&service);
    window.show();
    window.findChild<QLineEdit *>("imagePath")->setText(path);
    window.findChild<QPushButton *>("openImage")->click();
    window.findChild<QComboBox *>("recoveryMethod")->setCurrentIndex(2);
    window.findChild<QLineEdit *>("recoveryOutput")->setText(guiOut);
    window.findChild<QPushButton *>("startImageRecovery")->click();
    QTRY_COMPARE_WITH_TIMEOUT(
        window.findChild<QLabel *>("resultStatus")->text(),
        QString("Completed"), 5000);
    QTRY_VERIFY(
        window.findChild<QPushButton *>("recoverySettings")->isEnabled());
    window.close();
    QProcess process;
    process.start(QCoreApplication::applicationDirPath() + "/RecoverySuite_CLI",
                  {"image", "files", path, "--output", cliOut});
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitCode(), 0);
    QCOMPARE(load(cliOut + "/FRAG.TXT"), load(guiOut + "/FRAG.TXT"));
    QCOMPARE(load(cliOut + "/FRAG.TXT"), volume.expected());
    const auto source = service.openImage(path.toStdString());
    bool cancel = false;
    const auto cancelled = service.executeOperation(
        operation(source.devicePath, root.path(), 0, volume.sectors),
        [&](const auto &progress) {
          if (progress.processedBytes > 512 + 2 * 9 * 512 + 2 * 512 + 512)
            cancel = true;
        },
        [&] { return cancel; });
    QVERIFY(cancelled.cancelled);
    QVERIFY(!cancelled.success);
    QVERIFY(!cancelled.outputWritten);
    QVERIFY(!QFile::exists(root.filePath("FRAG.TXT")));
#endif
  }
};
QTEST_MAIN(TestFatFiles)
#include "test_fat_files.moc"
