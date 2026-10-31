#include "Application/Service/GUIRecoveryService.h"
#include "Application/Service/recovery_session.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
using namespace recoverysuite::application::service;
class TestRecoverySession : public QObject {
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
  static QByteArray image() {
    QByteArray b(2 * 65536, char{0});
    const QByteArray jpeg = QByteArray::fromHex("ffd8ff0102030405ffd9");
    b.replace(0, jpeg.size(), jpeg);
    b.replace(65536, jpeg.size(), jpeg);
    return b;
  }
  static models::RecoveryOperation operation(const QString &root) {
    models::RecoveryOperation op;
    op.operationType = "carving";
    op.targetDevicePath = (root + "/source.img").toStdString();
    op.outputPath = (root + "/out").toStdString();
    op.startSector = 0;
    op.numSectors = 256;
    op.verifyAfterRecovery = true;
    op.sessionPath = (root + "/session.json").toStdString();
    return op;
  }
private slots:
  void concurrentSessionLock() {
    QTemporaryDir root;
    QVERIFY(QDir().mkdir(root.filePath("out")));
    save(root.filePath("source.img"), image());
    GUIRecoveryService service(nullptr);
    service.openImage(root.filePath("source.img").toStdString());
    const auto op = operation(root.path());
    QLockFile lock(QString::fromStdString(op.sessionPath) + ".lock");
    QVERIFY(lock.tryLock());
    const auto result = service.executeOperation(op);
    QVERIFY(!result.success);
    QVERIFY(QDir(root.filePath("out")).entryList(QDir::Files).isEmpty());
  }

  void cancellationResumeAndCli() {
    QTemporaryDir root;
    QVERIFY(QDir().mkdir(root.filePath("out")));
    save(root.filePath("source.img"), image());
    GUIRecoveryService service(nullptr);
    service.openImage(root.filePath("source.img").toStdString());
    auto op = operation(root.path());
    bool stop = false;
    const auto first = service.executeOperation(
        op,
        [&](const auto &p) {
          if (p.recoveredItemsCount == 1)
            stop = true;
        },
        [&] { return stop; });
    QVERIFY(first.cancelled);
    QCOMPARE(first.recoveredItemsCount, uint64_t{1});
    QCOMPARE(RecoverySession::inspect(op.sessionPath).status,
             std::string("cancelled"));
    const auto name = QString::fromStdString(first.recoveredItems.front().name);
    const auto before = read(root.filePath("out/") + name);
    const auto timestamp =
        QFileInfo(root.filePath("out/") + name).lastModified();
    auto saved = RecoverySession::inspect(op.sessionPath);
    const auto resumed = service.executeOperation(saved.operation);
    QVERIFY2(resumed.success, resumed.errorMessage.c_str());
    QCOMPARE(resumed.recoveredItemsCount, uint64_t{2});
    QCOMPARE(resumed.details.at("Earlier outputs verified"), std::string("1"));
    QCOMPARE(resumed.details.at("New outputs saved"), std::string("1"));
    QCOMPARE(read(root.filePath("out/") + name), before);
    QCOMPARE(QFileInfo(root.filePath("out/") + name).lastModified(), timestamp);
    QCOMPARE(read(root.filePath("source.img")), image());
    QProcess cli;
    cli.start(QCoreApplication::applicationDirPath() + "/RecoverySuite_CLI",
              {"session", "resume", QString::fromStdString(op.sessionPath)});
    QVERIFY(cli.waitForFinished(10000));
    QCOMPARE(cli.exitCode(), 0);
    QVERIFY(
        cli.readAllStandardOutput().contains("Earlier outputs verified: 2"));
  }
  void rejectsTampering_data() {
    QTest::addColumn<QString>("fault");
    for (const char *f : {"source", "output", "schema", "engine", "duplicate",
                          "configuration", "truncated", "source-as-session"})
      QTest::newRow(f) << QString(f);
  }
  void rejectsTampering() {
    QFETCH(QString, fault);
    QTemporaryDir root;
    QVERIFY(QDir().mkdir(root.filePath("out")));
    save(root.filePath("source.img"), image());
    GUIRecoveryService service(nullptr);
    service.openImage(root.filePath("source.img").toStdString());
    auto op = operation(root.path());
    if (fault == "source-as-session") {
      op.sessionPath = op.targetDevicePath;
      const auto r = service.executeOperation(op);
      QVERIFY(!r.success);
      QCOMPARE(read(root.filePath("source.img")), image());
      return;
    }
    const auto first = service.executeOperation(op);
    QVERIFY2(first.success, first.errorMessage.c_str());
    const auto name = QString::fromStdString(first.recoveredItems.front().name);
    auto doc =
        QJsonDocument::fromJson(read(root.filePath("session.json"))).object();
    if (fault == "source") {
      save(root.filePath("other.img"), image());
      service.openImage(root.filePath("other.img").toStdString());
      auto b = image();
      b[b.size() - 1] = 1;
      save(root.filePath("source.img"), b);
      service.openImage(root.filePath("source.img").toStdString());
    }
    if (fault == "output")
      save(root.filePath("out/") + name, "CHANGED");
    if (fault == "schema")
      doc["schema"] = 999;
    if (fault == "engine")
      doc["engine"] = "future";
    if (fault == "duplicate") {
      auto a = doc["entries"].toArray();
      a.append(a.first());
      doc["entries"] = a;
    }
    if (fault == "configuration")
      op.startSector = 1;
    if (fault != "output" && fault != "source")
      save(root.filePath("session.json"), fault == "truncated"
                                              ? QByteArray("{")
                                              : QJsonDocument(doc).toJson());
    op.resumeSession = true;
    const auto result = service.executeOperation(op);
    QVERIFY(!result.success);
    if (fault == "output")
      QCOMPARE(read(root.filePath("out/") + name), QByteArray("CHANGED"));
  }
  void pendingAndMissingOutputs() {
    QTemporaryDir root;
    QVERIFY(QDir().mkdir(root.filePath("out")));
    save(root.filePath("source.img"), image());
    GUIRecoveryService service(nullptr);
    service.openImage(root.filePath("source.img").toStdString());
    auto op = operation(root.path());
    const auto first = service.executeOperation(op);
    QVERIFY(first.success);
    auto obj =
        QJsonDocument::fromJson(read(root.filePath("session.json"))).object();
    auto entries = obj["entries"].toArray();
    auto e = entries[0].toObject();
    e["complete"] = false;
    entries[0] = e;
    obj["entries"] = entries;
    save(root.filePath("session.json"), QJsonDocument(obj).toJson());
    const auto missing =
        root.filePath("out/") +
        QString::fromStdString(first.recoveredItems.back().name);
    QVERIFY(QFile::remove(missing));
    op.resumeSession = true;
    const auto result = service.executeOperation(op);
    QVERIFY2(result.success, result.errorMessage.c_str());
    QCOMPARE(result.details.at("Earlier outputs verified"), std::string("1"));
    QVERIFY(QFileInfo::exists(missing));
  }
  void initialCliSession() {
    QTemporaryDir root;
    QVERIFY(QDir().mkdir(root.filePath("out")));
    save(root.filePath("source.img"), image());
    QProcess cli;
    cli.start(QCoreApplication::applicationDirPath() + "/RecoverySuite_CLI",
              {"image", "carve", root.filePath("source.img"), "--output",
               root.filePath("out"), "--session", root.filePath("saved.json")});
    QVERIFY(cli.waitForFinished(10000));
    QCOMPARE(cli.exitCode(), 0);
    QCOMPARE(RecoverySession::inspect(root.filePath("saved.json").toStdString())
                 .status,
             std::string("completed"));
  }
};
QTEST_GUILESS_MAIN(TestRecoverySession)
#include "test_recovery_session.moc"
