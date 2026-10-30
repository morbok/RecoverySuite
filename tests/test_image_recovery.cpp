#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCryptographicHash>
#include <filesystem>
#include <limits>
#include "Disk/Access/disk_image_reader.h"
#include "Application/Service/GUIRecoveryService.h"
#include "Application/Service/safe_file_exporter.h"
#include "GUI/core/MainWindow.h"
#include "GUI/widgets/SourceDiscoveryWidget.h"
#include "GUI/widgets/RecoveryResultsWidget.h"
#ifndef _WIN32
#include <sys/stat.h>
#endif
using recoverysuite::disk::DiskImageReader;
using recoverysuite::application::service::GUIRecoveryService;
using recoverysuite::application::service::SafeFileExporter;
using recoverysuite::application::service::models::RecoveryOperation;
class TestImageRecovery : public QObject {
    Q_OBJECT
    static void save(const QString& path, const QByteArray& data) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) throw std::runtime_error("Fixture write failed");
    }
    static QByteArray load(const QString& path) {
        QFile file(path); if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Fixture read failed");
        return file.readAll();
    }
    static QByteArray jpeg() { return QByteArray::fromHex("ffd8ff") + "fixture-jpeg-payload" + QByteArray::fromHex("ffd9"); }
    static QByteArray pdf() { return "%PDF-1.7\nfixture-pdf-payload\n%%EOF"; }
    static QByteArray zip() {
        QByteArray data = QByteArray::fromHex("504b0304") + QByteArray(40, 'z');
        QByteArray end(22, char{0}); end.replace(0,4,QByteArray::fromHex("504b0506"));
        end[20] = 3; return data + end + "zip"; // EOCD comment included in recovered bytes.
    }
    static QString fixture(const QTemporaryDir& root) {
        QByteArray image(512 * 300, char{0});
        image.replace(10,jpeg().size(),jpeg()); // Decoy before the selected start sector.
        image.replace(512 + 65535,jpeg().size(),jpeg()); // JPEG header spans a read boundary.
        image.replace(70000,pdf().size(),pdf());
        image.replace(512 + 131070,zip().size(),zip()); // ZIP header spans another boundary.
        const auto path = root.filePath("source.img"); save(path,image); return path;
    }
    static RecoveryOperation operation(const std::string& source, const QString& destination, uint64_t start=1, uint64_t count=299) {
        RecoveryOperation op{}; op.operationType="carving"; op.targetDevicePath=source;
        op.startSector=start; op.numSectors=count; op.outputPath=destination.toStdString(); op.verifyAfterRecovery=true; return op;
    }
    template<class Action> static bool rejects(Action action) {
        try { action(); } catch (const std::exception&) { return true; } return false;
    }
private slots:
    void readOnlyImageAndBounds() {
        QTemporaryDir root; QVERIFY(root.isValid()); const auto source=fixture(root);
        const auto original=load(source);
        DiskImageReader reader; reader.open(source.toStdString());
        QVERIFY(reader.isOpen()); QCOMPARE(reader.getTotalSectors(),uint64_t{300}); QVERIFY(reader.getDiskInfo().isReadOnly);
        std::vector<std::byte> bytes;
        QVERIFY(reader.readSectors(299,1,bytes)); QCOMPARE(bytes.size(),size_t{512});
        QVERIFY(!reader.readSectors(300,1,bytes)); QVERIFY(bytes.empty());
        QVERIFY(!reader.readSectors(std::numeric_limits<uint64_t>::max(),2,bytes));
        QVERIFY(!reader.readSectors(0,0,bytes));
        QVERIFY(rejects([&]{ reader.writeSectors(0,1,bytes); }));
        QVERIFY(rejects([&]{ reader.open(root.path().toStdString()); }));
        QVERIFY(reader.isOpen()); // Failed replacement keeps the original open source.
        save(root.filePath("odd.img"),QByteArray(513,'x'));
        QVERIFY(rejects([&]{ reader.open(root.filePath("odd.img").toStdString()); }));
        QCOMPARE(load(source),original);
        reader.close(); QVERIFY(rejects([&]{ reader.readSectors(0,1,bytes); }));
#ifndef _WIN32
        std::filesystem::create_symlink(source.toStdString(),root.filePath("link.img").toStdString());
        QVERIFY(rejects([&]{ reader.open(root.filePath("link.img").toStdString()); }));
        QVERIFY(rejects([&]{ reader.open("/dev/null"); }));
        QVERIFY(::mkfifo(root.filePath("fifo").toStdString().c_str(),0600)==0);
        QVERIFY(rejects([&]{ reader.open(root.filePath("fifo").toStdString()); }));
#endif
    }
    void replacedAndTruncatedSourcesAreRejected() {
#ifndef _WIN32
        QTemporaryDir root; const auto source=fixture(root); DiskImageReader reader; reader.open(source.toStdString());
        QVERIFY(QFile::rename(source,root.filePath("old.img"))); save(source,QByteArray(512*300,'r'));
        std::vector<std::byte> bytes; QVERIFY(rejects([&]{ reader.readSectors(0,1,bytes); }));
        reader.open(source.toStdString());
        QFile changed(source); QVERIFY(changed.open(QIODevice::WriteOnly)); QVERIFY(changed.resize(512)); changed.close();
        QVERIFY(rejects([&]{ reader.readSectors(0,1,bytes); }));
        QVERIFY(rejects([&]{ reader.open(std::string("bad\0path",8)); }));
#else
        QSKIP("POSIX source identity validation");
#endif
    }
    void sourceToSavedBytes() {
#ifndef _WIN32
        QTemporaryDir root; const auto source=fixture(root);
        const auto before=QCryptographicHash::hash(load(source),QCryptographicHash::Sha256);
        const auto output=root.filePath("output"); QVERIFY(QDir().mkdir(output));
        GUIRecoveryService service(nullptr); QVERIFY(service.enumerateStorageDevices().empty());
        const auto opened=service.openImage(source.toStdString());
        QCOMPARE(service.enumerateStorageDevices().size(),size_t{1});
        auto op=operation(opened.devicePath,output);
        QVERIFY(service.validateOperation(op).empty());
        uint64_t last=0; bool sawProgress=false, blockedSourceChange=false;
        const auto result=service.executeOperation(op,[&](const auto& progress) {
            if (progress.totalBytes) {
                QVERIFY(progress.processedBytes>=last); last=progress.processedBytes; sawProgress=true;
                blockedSourceChange=rejects([&]{ service.openImage(source.toStdString()); });
            }
        });
        QVERIFY2(result.success,result.errorMessage.c_str()); QVERIFY(result.outputWritten); QVERIFY(sawProgress); QVERIFY(blockedSourceChange);
        QCOMPARE(result.processedBytes,uint64_t{299*512}); QCOMPARE(result.recoveredItemsCount,uint64_t{3});
        QCOMPARE(result.recoveredBytes,uint64_t(jpeg().size()+pdf().size()+zip().size()));
        QCOMPARE(load(output+"/carved_0.jpg"),jpeg()); QCOMPARE(load(output+"/carved_1.pdf"),pdf()); QCOMPARE(load(output+"/carved_2.zip"),zip());
        QCOMPARE(QCryptographicHash::hash(load(source),QCryptographicHash::Sha256),before);
        // Existing output is never overwritten, including when all candidates collide.
        save(output+"/carved_0.jpg","KEEP"); const auto again=service.executeOperation(op);
        QVERIFY(!again.success); QCOMPARE(again.failedItemsCount,uint64_t{3}); QVERIFY(!again.outputWritten);
        QCOMPARE(load(output+"/carved_0.jpg"),QByteArray("KEEP"));
        const auto partialOutput=root.filePath("partial"); QVERIFY(QDir().mkdir(partialOutput));
        save(partialOutput+"/carved_0.jpg","EXISTING"); op.outputPath=partialOutput.toStdString();
        const auto partial=service.executeOperation(op);
        QVERIFY(!partial.success); QVERIFY(partial.outputWritten); QCOMPARE(partial.recoveredItemsCount,uint64_t{2});
        QCOMPARE(partial.failedItemsCount,uint64_t{1}); QCOMPARE(load(partialOutput+"/carved_0.jpg"),QByteArray("EXISTING"));
#else
        QSKIP("Safe file export is POSIX-only");
#endif
    }
    void invalidOperationsAndDestinations() {
#ifndef _WIN32
        QTemporaryDir root; const auto source=fixture(root);
        GUIRecoveryService service(nullptr); const auto opened=service.openImage(source.toStdString());
        auto op=operation(opened.devicePath,root.path());
        auto wrong=op; wrong.targetDevicePath=root.filePath("missing.img").toStdString(); QVERIFY(!service.validateOperation(wrong).empty());
        wrong=op; wrong.startSector=std::numeric_limits<uint64_t>::max(); QVERIFY(!service.validateOperation(wrong).empty());
        wrong=op; wrong.numSectors=0; QVERIFY(!service.validateOperation(wrong).empty());
        wrong=op; wrong.operationType="file_recovery"; QVERIFY(!service.validateOperation(wrong).empty());
        wrong=op; wrong.createLogFile=true; QVERIFY(!service.validateOperation(wrong).empty());
        wrong=op; wrong.outputPath=source.toStdString(); QVERIFY(!service.validateOperation(wrong).empty());
        std::filesystem::create_directory_symlink(root.path().toStdString(),root.filePath("alias").toStdString());
        wrong=op; wrong.outputPath=root.filePath("alias").toStdString(); QVERIFY(!service.validateOperation(wrong).empty());
        QVERIFY(rejects([&]{ service.openImage(root.filePath("missing.img").toStdString()); }));
        QVERIFY(service.validateOperation(op).empty());
        const auto bad=service.executeOperation(wrong); QVERIFY(!bad.success); QVERIFY(!bad.outputWritten);
#else
        QSKIP("Safe file export is POSIX-only");
#endif
    }
    void exporterRejectsTraversalLinksAndCleansCancellation() {
#ifndef _WIN32
        QTemporaryDir root; SafeFileExporter exporter(root.path().toStdString());
        const std::vector<uint8_t> data{1,2,3};
        for (const auto& name: std::vector<std::string>{"../escape","/absolute","sub/file","sub\\file","..",std::string("nul\0name",8)})
            QVERIFY(rejects([&]{ exporter.write(name,data,true); }));
        const auto existing=root.filePath("existing"); save(existing,"PRESERVE");
        QVERIFY(rejects([&]{ exporter.write("existing",data,true); })); QCOMPARE(load(existing),QByteArray("PRESERVE"));
        std::filesystem::create_symlink(existing.toStdString(),root.filePath("symlink").toStdString());
        std::filesystem::create_hard_link(existing.toStdString(),root.filePath("hardlink").toStdString());
        QVERIFY(rejects([&]{ exporter.write("symlink",data,true); })); QVERIFY(rejects([&]{ exporter.write("hardlink",data,true); }));
        int polls=0;
        QVERIFY(rejects([&]{ exporter.write("cancelled",std::vector<uint8_t>(200000,42),true,[&]{return ++polls>=3;}); }));
        QVERIFY(!QFile::exists(root.filePath("cancelled"))); QCOMPARE(load(existing),QByteArray("PRESERVE"));
        QTemporaryDir outer; const auto anchored=outer.filePath("out"); const auto moved=outer.filePath("moved");
        QVERIFY(QDir().mkdir(anchored)); SafeFileExporter anchoredExporter(anchored.toStdString());
        QVERIFY(QDir().rename(anchored,moved)); std::filesystem::create_directory_symlink(root.path().toStdString(),anchored.toStdString());
        anchoredExporter.write("anchored",data,true);
        QVERIFY(QFile::exists(moved+"/anchored")); QVERIFY(!QFile::exists(root.filePath("anchored")));
#else
        QSKIP("Safe file export is POSIX-only");
#endif
    }
    void cooperativeCancellationAndEmptyScan() {
#ifndef _WIN32
        QTemporaryDir root; const auto source=fixture(root); GUIRecoveryService service(nullptr);
        const auto opened=service.openImage(source.toStdString()); auto op=operation(opened.devicePath,root.path());
        bool cancel=false;
        const auto result=service.executeOperation(op,[&](const auto& progress){if(progress.processedBytes>=131072) cancel=true;},[&]{return cancel;});
        QVERIFY(result.cancelled); QVERIFY(!result.success); QVERIFY(result.processedBytes<op.numSectors*512);
        QCOMPARE(result.recoveredItemsCount,uint64_t{2}); QCOMPARE(load(root.filePath("carved_0.jpg")),jpeg());
        save(root.filePath("empty.img"),QByteArray(4096,char{0}));
        const auto empty=service.openImage(root.filePath("empty.img").toStdString());
        op=operation(empty.devicePath,root.path(),0,8); const auto noFiles=service.executeOperation(op);
        QVERIFY(noFiles.success); QCOMPARE(noFiles.recoveredItemsCount,uint64_t{0}); QVERIFY(!noFiles.outputWritten);
#else
        QSKIP("Safe file export is POSIX-only");
#endif
    }
    void fourKAndIncompleteCandidates() {
#ifndef _WIN32
        QTemporaryDir root; QByteArray data(4096*3,char{0}); data.replace(4099,jpeg().size(),jpeg());
        const auto path=root.filePath("4k.img"); save(path,data); GUIRecoveryService service(nullptr);
        const auto source=service.openImage(path.toStdString(),4096); QCOMPARE(source.bytesPerSector,uint32_t{4096});
        auto op=operation(source.devicePath,root.path(),1,2); const auto result=service.executeOperation(op);
        QVERIFY2(result.success,result.errorMessage.c_str()); QCOMPARE(load(root.filePath("carved_0.jpg")),jpeg());
        data=QByteArray(512,char{0}); data.replace(0,5,"%PDF-"); save(root.filePath("incomplete.img"),data);
        const auto incomplete=service.openImage(root.filePath("incomplete.img").toStdString());
        op=operation(incomplete.devicePath,root.path(),0,1); const auto skipped=service.executeOperation(op);
        QVERIFY(!skipped.success); QVERIFY(!skipped.outputWritten); QVERIFY(!QFile::exists(root.filePath("carved_0.pdf")));
        save(root.filePath("short-jpeg.img"), QByteArray(509, char{0}) + QByteArray::fromHex("ffd8ff"));
        const auto shortJpeg=service.openImage(root.filePath("short-jpeg.img").toStdString());
        op=operation(shortJpeg.devicePath,root.path(),0,1); const auto shortResult=service.executeOperation(op);
        QVERIFY(!shortResult.success); QVERIFY(!shortResult.outputWritten);
        QCOMPARE(shortResult.details.at("Skipped incomplete or oversized candidates"),std::string("1"));
        QByteArray oversized(17*1024*1024,char{0}); oversized.replace(0,5,"%PDF-");
        save(root.filePath("oversized.img"),oversized);
        const auto large=service.openImage(root.filePath("oversized.img").toStdString());
        op=operation(large.devicePath,root.path(),0,uint64_t(oversized.size()/512));
        const auto bounded=service.executeOperation(op); QVERIFY(!bounded.success); QVERIFY(!bounded.outputWritten);
        QCOMPARE(bounded.details.at("Skipped incomplete or oversized candidates"),std::string("1"));
#else
        QSKIP("Safe file export is POSIX-only");
#endif
    }
    void completeGuiWorkflow() {
#ifndef _WIN32
        QTemporaryDir root; const auto source=fixture(root); GUIRecoveryService service(nullptr);
        recoverysuite::gui::core::MainWindow window(&service); window.show();
        window.findChild<QLineEdit*>("imagePath")->setText(source);
        QTest::mouseClick(window.findChild<QPushButton*>("openImage"),Qt::LeftButton);
        window.findChild<QLineEdit*>("recoveryStart")->setText("1");
        window.findChild<QLineEdit*>("recoveryCount")->setText("299");
        window.findChild<QLineEdit*>("recoveryOutput")->setText(root.path());
        QTest::mouseClick(window.findChild<QPushButton*>("startImageRecovery"),Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(window.findChild<QLabel*>("resultStatus")->text(),QString("Completed"),5000);
        QCOMPARE(window.findChild<QLabel*>("resultExport")->text(),QString("Written to destination"));
        QCOMPARE(load(root.filePath("carved_0.jpg")),jpeg());
        QTRY_VERIFY(window.findChild<QPushButton*>("recoverySettings")->isEnabled());
        QTest::mouseClick(window.findChild<QPushButton*>("recoverySettings"),Qt::LeftButton);
        window.findChild<QLineEdit*>("recoveryStart")->setText("-1");
        QTest::mouseClick(window.findChild<QPushButton*>("startImageRecovery"),Qt::LeftButton);
        QVERIFY(window.findChild<QLabel*>("configurationStatus")->text().contains("nonnegative"));
        window.close();
#else
        QSKIP("Safe file export is POSIX-only");
#endif
    }
    void closeDuringRecoveryWaitsForCleanup() {
#ifndef _WIN32
        QTemporaryDir root; const auto source=fixture(root); GUIRecoveryService service(nullptr);
        recoverysuite::gui::core::MainWindow window(&service); window.show();
        window.findChild<QLineEdit*>("imagePath")->setText(source);
        QTest::mouseClick(window.findChild<QPushButton*>("openImage"),Qt::LeftButton);
        window.findChild<QLineEdit*>("recoveryOutput")->setText(root.path());
        QTest::mouseClick(window.findChild<QPushButton*>("startImageRecovery"),Qt::LeftButton);
        window.close();
        QTRY_VERIFY_WITH_TIMEOUT(!window.isVisible(),5000);
#else
        QSKIP("Safe file export is POSIX-only");
#endif
    }

};
QTEST_MAIN(TestImageRecovery)
#include "test_image_recovery.moc"
