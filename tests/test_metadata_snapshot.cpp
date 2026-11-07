#include <QTest>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProcess>
#include <limits>
#include "Application/Service/GUIRecoveryService.h"
#include "Disk/Access/disk_image_reader.h"
#include "Recovery/MetadataRecovery.h"
#include "Recovery/FileRecovery.h"
#include "Recovery/source_bytes.h"
#include "Recovery/FilesystemAnalyzer.h"
#include "GUI/core/MainWindow.h"
using namespace recoverysuite;
class TestMetadataSnapshot : public QObject {
    Q_OBJECT
    static void le(QByteArray& bytes,int offset,uint64_t value,int width) {
        for(int i=0;i<width;++i)bytes[offset+i]=static_cast<char>((value>>(8*i))&255);
    }
    static void save(const QString& path,const QByteArray& bytes){QFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size())throw std::runtime_error("Fixture write failed");}
    static QByteArray load(const QString& path){QFile file(path);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Fixture read failed");return file.readAll();}
    static QByteArray volume(int sectorSize=512,int sectors=2880,int fatSize=9,int roots=224) {
        QByteArray data(sectorSize*sectors,char{0});data[0]=char(0xeb);data[2]=char(0x90);
        le(data,11,sectorSize,2);data[13]=1;le(data,14,1,2);data[16]=2;
        le(data,17,roots,2);le(data,19,sectors,2);le(data,22,fatSize,2);data[510]=char(0x55);data[511]=char(0xaa);
        data.replace(sectorSize,fatSize*sectorSize,QByteArray(fatSize*sectorSize,char(0x33)));
        data.replace((1+fatSize)*sectorSize,fatSize*sectorSize,QByteArray(fatSize*sectorSize,char(0x44)));
        data.replace((1+2*fatSize)*sectorSize,roots*32,QByteArray(roots*32,char(0x55)));return data;
    }
    static application::service::models::RecoveryOperation operation(const std::string& source,const QString& output,uint64_t start,uint64_t count) {
        application::service::models::RecoveryOperation op{};op.operationType="metadata_recovery";op.targetDevicePath=source;
        op.outputPath=output.toStdString();op.startSector=start;op.numSectors=count;op.verifyAfterRecovery=true;return op;
    }
private slots:
    void exactFatSnapshots_data(){QTest::addColumn<int>("sectorSize");QTest::addColumn<int>("sectors");QTest::addColumn<int>("fatSize");QTest::addColumn<int>("roots");QTest::addColumn<QString>("type");
        QTest::newRow("fat12-512")<<512<<2880<<9<<224<<QString("FAT12");
        QTest::newRow("fat12-1024")<<1024<<2000<<3<<128<<QString("FAT12");
        QTest::newRow("fat16-4096")<<4096<<5000<<3<<128<<QString("FAT16");}
    void exactFatSnapshots(){
#ifndef _WIN32
        QFETCH(int,sectorSize);QFETCH(int,sectors);QFETCH(int,fatSize);QFETCH(int,roots);QFETCH(QString,type);
        QTemporaryDir root;const auto data=volume(sectorSize,sectors,fatSize,roots);const auto path=root.filePath("fat.img");
        const auto image=QByteArray(3*sectorSize,char(0x11))+data+QByteArray(sectorSize,char(0x22));save(path,image);
        application::service::GUIRecoveryService service(nullptr);const auto source=service.openImage(path.toStdString(),sectorSize);
        auto op=operation(source.devicePath,root.path(),3,sectors);const auto result=service.executeOperation(op);
        QVERIFY2(result.success,result.errorMessage.c_str());QCOMPARE(result.recoveredItemsCount,uint64_t{4});
        QCOMPARE(load(root.filePath(type+"_BootSector")),data.left(sectorSize));
        QCOMPARE(load(root.filePath(type+"_FAT0")),QByteArray(fatSize*sectorSize,char(0x33)));
        QCOMPARE(load(root.filePath(type+"_FAT1")),QByteArray(fatSize*sectorSize,char(0x44)));
        QCOMPARE(load(root.filePath("RootDirectory")),QByteArray(roots*32,char(0x55)));
        QCOMPARE(load(path),image);const auto again=service.executeOperation(op);QVERIFY(!again.success);QCOMPARE(again.failedItemsCount,uint64_t{4});
        QCOMPARE(load(root.filePath(type+"_FAT0")),QByteArray(fatSize*sectorSize,char(0x33)));
#endif
    }
    void selectedBoundsInvalidGeometryAndCancellation(){
#ifndef _WIN32
        QTemporaryDir root;const auto path=root.filePath("fat.img");auto data=volume();save(path,data);
        application::service::GUIRecoveryService service(nullptr);auto source=service.openImage(path.toStdString());
        auto op=operation(source.devicePath,root.path(),0,5);auto result=service.executeOperation(op);QVERIFY(!result.success);QVERIFY(!result.outputWritten);
        QVERIFY(!QFile::exists(root.filePath("FAT12_BootSector")));
        data[13]=0;save(path,data);source=service.openImage(path.toStdString());op=operation(source.devicePath,root.path(),0,2880);
        result=service.executeOperation(op);QVERIFY(!result.success);QVERIFY(!result.outputWritten);
        data=volume();data.replace(3,8,"NTFS    ");save(path,data);source=service.openImage(path.toStdString());op.targetDevicePath=source.devicePath;
        result=service.executeOperation(op);QVERIFY(!result.success);QVERIFY(!result.outputWritten);
        data=volume();save(path,data);source=service.openImage(path.toStdString());op.targetDevicePath=source.devicePath;int checks=0;
        result=service.executeOperation(op,{},[&]{return ++checks>5;});QVERIFY(result.cancelled);QVERIFY(!result.success);QVERIFY(!result.outputWritten);
        QCOMPARE(QDir(root.path()).entryList(QDir::Files).size(),1);
#endif
    }
    void byteSlicesAreExactAndBounded(){
        QTemporaryDir root;const auto path=root.filePath("bytes.img");QByteArray data(4096*8,char{0});
        for(qsizetype i=0;i<data.size();++i) data[i]=static_cast<char>(i%251);
        save(path,data);
        disk::DiskImageReader reader(4096);reader.open(path.toStdString());recovery::RecoveryCapabilityRegistry registry;
        registry.registerCapability(recovery::RecoveryCapability::METADATA_RECOVERY);registry.registerCapability(recovery::RecoveryCapability::FILE_RECOVERY);
        recovery::RecoverySafetyPolicy policy;recovery::MetadataRecovery metadata(registry,policy,&reader);
        std::vector<uint8_t> bytes;QVERIFY(metadata.recoverMetadataType(true,true,1,3,true,true,true,true,"raw",4095,4097,bytes));
        QCOMPARE(QByteArray(reinterpret_cast<const char*>(bytes.data()),bytes.size()),data.mid(4096+4095,4097));
        QVERIFY(!metadata.recoverMetadataType(true,true,1,3,true,true,true,true,"raw",3*4096-1,2,bytes));QVERIFY(bytes.empty());
        QVERIFY(!metadata.recoverMetadataType(true,true,1,3,true,true,true,true,"raw",std::numeric_limits<uint64_t>::max(),1,bytes));
        QVERIFY(!metadata.recoverMetadataType(true,true,1,3,true,true,true,true,"raw",0,std::numeric_limits<uint64_t>::max(),bytes));
        recovery::FileRecovery files(registry,policy,&reader);QVERIFY(files.recoverFile(true,true,1,3,true,true,true,true,"raw",2,4097,bytes));
        QCOMPARE(QByteArray(reinterpret_cast<const char*>(bytes.data()),bytes.size()),data.mid(8192,4097));
        QVERIFY(!files.recoverFile(true,true,1,3,true,true,true,true,"raw",0,1,bytes));QVERIFY(bytes.empty());
        QVERIFY(!files.recoverFile(true,true,1,3,true,true,true,true,"raw",3,4097,bytes));
    }
    void shortOversizedAndFailedReadsAreRejected(){
        class FaultReader : public disk::IDiskReader {
        public:
            int delta=0,calls=0;bool ok=true;
            void open(const std::string&) override{} void close() override{}
            bool isOpen() const noexcept override{return true;}
            bool readSectors(uint64_t,uint64_t count,std::vector<std::byte>& out) override{++calls;out.assign(count*4096+delta,std::byte{0x42});return ok;}
            bool writeSectors(uint64_t,uint64_t,const std::vector<std::byte>&) override{return false;}
            disk::DeviceInformation getDiskInfo() const override{return {};}
            uint32_t getSectorSize() const noexcept override{return 4096;}
            uint64_t getTotalSectors() const noexcept override{return 100000;}
        } reader;
        std::vector<uint8_t> bytes{1,2,3};
        for(const int delta:{-1,1}){reader.delta=delta;QVERIFY(!recovery::readSourceBytes(&reader,0,8,4095,100,bytes));QVERIFY(bytes.empty());}
        reader.delta=0;reader.ok=false;QVERIFY(!recovery::readSourceBytes(&reader,0,8,4095,100,bytes));QVERIFY(bytes.empty());
        reader.ok=true;reader.delta=-1;recovery::RecoveryCapabilityRegistry registry;
        registry.registerCapability(recovery::RecoveryCapability::FILESYSTEM_ANALYSIS);recovery::RecoverySafetyPolicy policy;
        recovery::FilesystemAnalyzer analyzer(registry,policy,&reader);std::map<std::string,std::string> fields;
        QVERIFY(!analyzer.analyzeFilesystem(true,true,0,1,true,true,true,true,fields));QVERIFY(fields.empty());
        reader.delta=0;const int calls=reader.calls;QVERIFY(!recovery::readSourceBytes(&reader,0,100000,0,16*1024*1024+1,bytes));
        QCOMPARE(reader.calls,calls);QVERIFY(bytes.empty());
    }
    void fat32SnapshotsDoNotInventRootChain(){
#ifndef _WIN32
        QTemporaryDir root;const auto path=root.filePath("fat32.img");QByteArray boot(512,char{0});
        le(boot,11,512,2);boot[13]=1;le(boot,14,32,2);boot[16]=2;le(boot,32,70000,4);le(boot,36,600,4);le(boot,44,2,4);
        boot[510]=char(0x55);boot[511]=char(0xaa);QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));QVERIFY(file.resize(70000*512));
        QCOMPARE(file.write(boot),qint64{512});QVERIFY(file.seek(32*512));QCOMPARE(file.write(QByteArray(600*512,char(0x66))),qint64{600*512});
        QVERIFY(file.seek(632*512));QCOMPARE(file.write(QByteArray(600*512,char(0x77))),qint64{600*512});file.close();
        application::service::GUIRecoveryService service(nullptr);const auto source=service.openImage(path.toStdString());
        const auto result=service.executeOperation(operation(source.devicePath,root.path(),0,70000));QVERIFY2(result.success,result.errorMessage.c_str());
        QCOMPARE(result.recoveredItemsCount,uint64_t{3});QCOMPARE(load(root.filePath("FAT32_FAT1")),QByteArray(600*512,char(0x77)));
        QVERIFY(!QFile::exists(root.filePath("RootDirectory")));
#endif
    }
    void bootAnalysisReportsOnlyObservedFields(){
        QTemporaryDir root;const auto path=root.filePath("boot.img");auto fat=volume();fat[38]=char(0x29);
        le(fat,39,0x12345678,4);fat.replace(43,11,"MY_VOLUME  ");save(path,QByteArray(3*512,char{0})+fat);
        application::service::GUIRecoveryService service(nullptr);auto source=service.openImage(path.toStdString());
        auto result=service.analyzeFilesystem(source.devicePath,3,1);QVERIFY2(result.success,result.errorMessage.c_str());
        QCOMPARE(result.filesystemInfo.type,std::string("FAT12"));QCOMPARE(result.filesystemInfo.totalSizeBytes,uint64_t{2880*512});
        QCOMPARE(result.properties.at("Free Space"),std::string("unknown"));QCOMPARE(result.properties.at("Used Space"),std::string("unknown"));
        QCOMPARE(result.properties.at("Volume Serial Number"),std::to_string(0x12345678));QCOMPARE(result.properties.at("Volume Label"),std::string("MY_VOLUME"));
        QVERIFY(!service.analyzeFilesystem(root.filePath("missing.img").toStdString(),3,1).success);
        QProcess process;process.start(QCoreApplication::applicationDirPath()+"/RecoverySuite_CLI",{"image","analyze",path,"--start","3","--count","1"});
        QVERIFY(process.waitForFinished(5000));QCOMPARE(process.exitCode(),0);const auto text=process.readAllStandardOutput();
        QVERIFY(text.contains("Filesystem Type: FAT12"));QVERIFY(text.contains("Free Space: unknown"));QVERIFY(text.contains("Boot-sector fields only"));
        QByteArray ntfs(512*64,char{0});ntfs.replace(3,8,"NTFS    ");le(ntfs,11,512,2);ntfs[13]=8;
        le(ntfs,40,64,8);le(ntfs,72,0x123456789abcdef0ULL,8);ntfs[510]=char(0x55);ntfs[511]=char(0xaa);
        const auto ntfsPath=root.filePath("ntfs.img");save(ntfsPath,ntfs);
        source=service.openImage(ntfsPath.toStdString());result=service.analyzeFilesystem(source.devicePath,0,1);QVERIFY(result.success);
        QCOMPARE(result.properties.at("Volume Serial Number"),std::to_string(0x123456789abcdef0ULL));
        QCOMPARE(result.properties.at("Version"),std::string("unknown"));QCOMPARE(result.filesystemInfo.totalSizeBytes,uint64_t{32768});
        le(ntfs,40,std::numeric_limits<uint64_t>::max(),8);const auto invalidPath=root.filePath("invalid-ntfs.img");save(invalidPath,ntfs);source=service.openImage(invalidPath.toStdString());
        result=service.analyzeFilesystem(source.devicePath,0,1);QVERIFY(result.success);QCOMPARE(result.properties.at("Total Size"),std::string("unknown"));
        QVERIFY(result.properties.count("Geometry Warning"));
        auto reader=std::make_shared<disk::DiskImageReader>();reader->open(invalidPath.toStdString());
        recovery::RecoveryCapabilityRegistry registry;registry.registerCapability(recovery::RecoveryCapability::FILESYSTEM_ANALYSIS);
        recovery::RecoverySafetyPolicy policy;recovery::FilesystemAnalyzer analyzer(registry,policy,reader.get());
        std::map<std::string,std::string> fields;
        QVERIFY(!analyzer.getFilesystemInfo(true,true,0,1,true,true,true,true,"FAT32",fields));QVERIFY(fields.empty());
    }
    void noSyntheticRecoveredFiles(){
        QTemporaryDir root;const auto path=root.filePath("fat.img");save(path,volume());disk::DiskImageReader reader;reader.open(path.toStdString());
        recovery::RecoveryCapabilityRegistry registry;registry.registerCapability(recovery::RecoveryCapability::METADATA_RECOVERY);
        registry.registerCapability(recovery::RecoveryCapability::FILE_RECOVERY);recovery::RecoverySafetyPolicy policy;
        recovery::FileRecovery files(registry,policy,&reader);std::map<std::string,std::vector<uint8_t>> recovered;
        QVERIFY(!files.recoverFiles(true,true,0,2880,true,true,true,true,recovered));QVERIFY(recovered.empty());
    }
    void guiAndCliUseMetadataMethod(){
#ifndef _WIN32
        QTemporaryDir root;const auto path=root.filePath("fat.img");save(path,volume());
        const auto guiOutput=root.filePath("gui");const auto cliOutput=root.filePath("cli");QVERIFY(QDir().mkdir(guiOutput));QVERIFY(QDir().mkdir(cliOutput));
        application::service::GUIRecoveryService service(nullptr);gui::core::MainWindow window(&service);window.show();
        window.findChild<QLineEdit*>("imagePath")->setText(path);window.findChild<QPushButton*>("openImage")->click();
        auto* method=window.findChild<QComboBox*>("recoveryMethod");QVERIFY(method);method->setCurrentIndex(1);
        window.findChild<QLineEdit*>("recoveryOutput")->setText(guiOutput);window.findChild<QPushButton*>("startImageRecovery")->click();
        QTRY_COMPARE_WITH_TIMEOUT(window.findChild<QLabel*>("resultStatus")->text(),QString("Completed"),5000);
        QTRY_VERIFY(window.findChild<QPushButton*>("recoverySettings")->isEnabled());
        QCOMPARE(load(guiOutput+"/FAT12_FAT1"),QByteArray(9*512,char(0x44)));window.close();
        QProcess process;process.start(QCoreApplication::applicationDirPath()+"/RecoverySuite_CLI",{"image","metadata",path,"--output",cliOutput});
        QVERIFY(process.waitForFinished(5000));QCOMPARE(process.exitStatus(),QProcess::NormalExit);QCOMPARE(process.exitCode(),0);
        QVERIFY(process.readAllStandardOutput().contains("Saved items: 4"));QCOMPARE(load(cliOutput+"/FAT12_FAT1"),load(guiOutput+"/FAT12_FAT1"));
#endif
    }
};
QTEST_MAIN(TestMetadataSnapshot)
#include "test_metadata_snapshot.moc"
