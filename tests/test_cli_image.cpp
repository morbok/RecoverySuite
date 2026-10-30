#include <QTest>
#include <QProcess>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <filesystem>
#ifndef _WIN32
#include <csignal>
#include <unistd.h>
#endif
class TestCLIImage : public QObject {
    Q_OBJECT
    struct Result { int exit; QByteArray out, error; QProcess::ExitStatus status; };
    static Result run(const QStringList& args) {
        QProcess process; process.start(QCoreApplication::applicationDirPath()+"/RecoverySuite_CLI",args);
        if (!process.waitForStarted(5000) || !process.waitForFinished(15000)) throw std::runtime_error("CLI process did not finish");
        return {process.exitCode(),process.readAllStandardOutput(),process.readAllStandardError(),process.exitStatus()};
    }
    static void save(const QString& path,const QByteArray& data) {
        QFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size())throw std::runtime_error("Fixture write failed");
    }
    static QByteArray load(const QString& path) {QFile file(path);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Fixture read failed");return file.readAll();}
    static QByteArray jpeg(){return QByteArray::fromHex("ffd8ff001122ffd9");}
    static QString fixture(const QTemporaryDir& root) {
        QByteArray image(512*20,char{0});image.replace(10,jpeg().size(),jpeg());image.replace(1024,jpeg().size(),jpeg());
        const auto path=root.filePath("image with spaces.img");save(path,image);return path;
    }
private slots:
    void infoAndStrictArguments() {
        QTemporaryDir root;const auto source=fixture(root);auto info=run({"image","info",source});
        QCOMPARE(info.status,QProcess::NormalExit);QCOMPARE(info.exit,0);QVERIFY(info.out.contains("Read-only: yes"));QVERIFY(info.out.contains("Sectors: 20"));
        const QList<QStringList> bad={{"image"},{"image","unknown",source},{"image","info",source,"--sector-size","-1"},
            {"image","info",source,"--sector-size","512","--sector-size","4096"},{"image","info",source,"--count","1"},
            {"image","info",source,"--sector-size","513"},{"image","info",source,"--sector-size"},
            {"image","carve",source},{"image","carve",source,"--output",root.path(),"--count","0"},
            {"image","carve",source,"--output",root.path(),"--start","18446744073709551615"},
            {"image","carve",source,"--output",root.path(),"--count","18446744073709551616"},
            {"image","carve",source,"--output",root.path(),"--no-verify","--no-verify"}};
        for(const auto& args:bad){const auto result=run(args);QCOMPARE(result.status,QProcess::NormalExit);QCOMPARE(result.exit,2);}
        QCOMPARE(run({"image","info",root.filePath("missing")}).exit,1);
        QCOMPARE(run({"disk","info","-1"}).exit,1);
        QCOMPARE(run({"cancel","fake-operation"}).exit,1);
        const auto legacy=run({"recover","file_recovery","0","0","100",root.path()});QCOMPARE(legacy.exit,1);QVERIFY(legacy.error.contains("unavailable"));
        QVERIFY(!QFile::exists(root.filePath("carved_0.jpg")));
    }
    void exportsActualBytesWithoutOverwrite() {
#ifndef _WIN32
        QTemporaryDir root;const auto source=fixture(root);const auto before=QCryptographicHash::hash(load(source),QCryptographicHash::Sha256);
        const QStringList args={"image","carve",source,"--output",root.path(),"--start","1"};
        const auto first=run(args);QCOMPARE(first.exit,0);QVERIFY(first.out.contains("Saved items: 1"));
        QVERIFY(first.out.contains("Processed bytes: 9728"));QCOMPARE(load(root.filePath("carved_0.jpg")),jpeg());
        QCOMPARE(QCryptographicHash::hash(load(source),QCryptographicHash::Sha256),before);
        const auto again=run(args);QCOMPARE(again.exit,1);QVERIFY(again.out.contains("Failed items: 1"));QCOMPARE(load(root.filePath("carved_0.jpg")),jpeg());
        const auto separate=root.filePath("another");QVERIFY(QDir().mkdir(separate));
        const auto noVerify=run({"image","carve",source,"--output",separate,"--start","1","--count","3","--no-verify"});
        QCOMPARE(noVerify.exit,0);QVERIFY(noVerify.out.contains("Verification: Not requested"));
        QCOMPARE(load(separate+"/carved_0.jpg"),jpeg());
#endif
    }
    void invalidOutputAndIncompleteCandidates() {
#ifndef _WIN32
        QTemporaryDir root;const auto source=fixture(root);
        QCOMPARE(run({"image","carve",source,"--output",source}).exit,1);
        const auto link=root.filePath("output-link");std::filesystem::create_directory_symlink(root.path().toStdString(),link.toStdString());
        QCOMPARE(run({"image","carve",source,"--output",link}).exit,1);
        save(root.filePath("incomplete.img"),QByteArray("%PDF-")+QByteArray(507,char{0}));
        const auto partial=run({"image","carve",root.filePath("incomplete.img"),"--output",root.path()});
        QCOMPARE(partial.exit,1);QVERIFY(partial.out.contains("Skipped incomplete or oversized candidates: 1"));
        QVERIFY(!QFile::exists(root.filePath("carved_0.pdf")));
#endif
    }
    void cooperativeSignals_data() {
        QTest::addColumn<int>("signal");
#ifndef _WIN32
        QTest::newRow("interrupt")<<int(SIGINT);QTest::newRow("terminate")<<int(SIGTERM);
#else
        QTest::newRow("unsupported")<<0;
#endif
    }
    void cooperativeSignals() {
#ifndef _WIN32
        QFETCH(int,signal);QTemporaryDir root;QFile source(root.filePath("large.img"));QVERIFY(source.open(QIODevice::WriteOnly));
        QVERIFY(source.resize(qint64{1024}*1024*1024));source.close();
        QProcess process;process.setReadChannel(QProcess::StandardError);
        process.start(QCoreApplication::applicationDirPath()+"/RecoverySuite_CLI",{"image","carve",source.fileName(),"--output",root.path()});
        QVERIFY(process.waitForStarted());QByteArray errors;QElapsedTimer timer;timer.start();
        while(!errors.contains("Progress: 0%")&&timer.elapsed()<5000){process.waitForReadyRead(100);errors+=process.readAllStandardError();}
        QVERIFY2(errors.contains("Progress: 0%"),errors.constData());QCOMPARE(::kill(static_cast<pid_t>(process.processId()),signal),0);
        QVERIFY(process.waitForFinished(5000));QCOMPARE(process.exitStatus(),QProcess::NormalExit);QCOMPARE(process.exitCode(),130);
        errors+=process.readAllStandardError();QVERIFY(errors.contains("Cancelled"));
        QCOMPARE(QDir(root.path()).entryList(QDir::Files).size(),1);
#endif
    }
};
QTEST_GUILESS_MAIN(TestCLIImage)
#include "test_cli_image.moc"
