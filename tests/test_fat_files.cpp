#include <QTest>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QProcess>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include "Application/Service/GUIRecoveryService.h"
#include "Application/Service/RecoveryService.h"
#include "GUI/core/MainWindow.h"
using namespace recoverysuite;
class TestFatFiles : public QObject {
    Q_OBJECT
    static void le(QByteArray& data,int pos,uint64_t value,int width=2){for(int i=0;i<width;++i)data[pos+i]=char((value>>(i*8))&255);}
    static void save(const QString& path,const QByteArray& data){QFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size())throw std::runtime_error("Fixture write failed");}
    static QByteArray load(const QString& path){QFile file(path);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Fixture read failed");return file.readAll();}
    struct Volume {
        int ss,sectors,fatSectors,rootStart,dataStart;bool fat16;QByteArray data;
        Volume(bool sixteen=false,int sectorSize=512):ss(sectorSize),sectors(sixteen?5000:2880),fatSectors(sixteen?3:9),
            rootStart(1+fatSectors*2),dataStart(rootStart+(32*32+ss-1)/ss),fat16(sixteen),data(sectors*ss,char{0}) {
            le(data,11,ss);data[13]=1;le(data,14,1);data[16]=2;le(data,17,32);le(data,19,sectors);data[21]=char(0xf8);le(data,22,fatSectors);
            data[510]=char(0x55);data[511]=char(0xaa);entry(0,sixteen?0xfff8:0xff8);entry(1,sixteen?0xffff:0xfff);
            entry(2,7);entry(7,3);entry(3,sixteen?0xffff:0xfff);
            directory(0,"FRAG    TXT",2,2*ss+17);directory(1,"EMPTY   TXT",0,0);
            directory(2,"DELETED TXT",0,0);data[rootStart*ss+2*32]=char(0xe5);
            directory(3,"SUBDIR     ",0,0);data[rootStart*ss+3*32+11]=char(0x10);
            data[(dataStart+0)*ss]='A';data.replace((dataStart+0)*ss,ss,QByteArray(ss,'A'));
            data.replace((dataStart+5)*ss,ss,QByteArray(ss,'B'));data.replace((dataStart+1)*ss,ss,QByteArray(ss,'C'));
        }
        void entry(int cluster,unsigned value){
            for(int copy=0;copy<2;++copy){const int base=(1+copy*fatSectors)*ss;
                if(fat16)le(data,base+cluster*2,value);
                else {const int pos=base+cluster+cluster/2;unsigned pair=static_cast<unsigned char>(data[pos])|(unsigned(static_cast<unsigned char>(data[pos+1]))<<8);
                    pair=(cluster&1)?((pair&0xf)|(value<<4)):((pair&0xf000)|value);le(data,pos,pair);}}
        }
        void directory(int index,const char* name,unsigned first,unsigned size){const int pos=rootStart*ss+index*32;data.replace(pos,11,QByteArray(name,11));data[pos+11]=char(0x20);le(data,pos+26,first);le(data,pos+28,size,4);}
        QByteArray expected()const{return QByteArray(ss,'A')+QByteArray(ss,'B')+QByteArray(17,'C');}
    };
    static application::service::models::RecoveryOperation operation(const std::string& path,const QString& output,uint64_t start,uint64_t count){
        application::service::models::RecoveryOperation op{};op.operationType="file_recovery";op.targetDevicePath=path;op.outputPath=output.toStdString();op.startSector=start;op.numSectors=count;op.verifyAfterRecovery=true;return op;}
private slots:
    void copiesFragmentedAllocatedFiles_data(){QTest::addColumn<bool>("sixteen");QTest::addColumn<int>("ss");QTest::addColumn<bool>("dirty");QTest::newRow("FAT12")<<false<<512<<false;QTest::newRow("FAT16-4k")<<true<<4096<<false;QTest::newRow("FAT16-dirty-flags")<<true<<4096<<true;}
    void copiesFragmentedAllocatedFiles(){
#ifndef _WIN32
        QFETCH(bool,sixteen);QFETCH(int,ss);QFETCH(bool,dirty);QTemporaryDir root;Volume volume(sixteen,ss);if(dirty)volume.entry(1,0x3fff);const auto path=root.filePath("source.img");
        const auto image=QByteArray(3*ss,char(0x7a))+volume.data;save(path,image);
        application::service::GUIRecoveryService service(nullptr);const auto source=service.openImage(path.toStdString(),ss);
        const auto op=operation(source.devicePath,root.path(),3,volume.sectors);const auto result=service.executeOperation(op);
        QVERIFY2(result.success,result.errorMessage.c_str());QCOMPARE(result.recoveredItemsCount,uint64_t{2});QCOMPARE(result.recoveredBytes,uint64_t(volume.expected().size()));
        QCOMPARE(load(root.filePath("FRAG.TXT")),volume.expected());QCOMPARE(load(root.filePath("EMPTY.TXT")),QByteArray());QCOMPARE(load(path),image);
        QCOMPARE(result.details.at("Deleted entries not reconstructed"),std::string("1"));QCOMPARE(result.details.at("Subdirectories not traversed"),std::string("1"));
        const auto repeated=service.executeOperation(op);QVERIFY(!repeated.success);QCOMPARE(repeated.failedItemsCount,uint64_t{2});QCOMPARE(load(root.filePath("FRAG.TXT")),volume.expected());
        auto reader=std::make_shared<disk::DiskImageReader>(ss);reader->open(path.toStdString());application::service::RecoveryService legacy(reader);
        const auto buffered=legacy.recoverFiles(3,volume.sectors);QVERIFY(buffered.success);QCOMPARE(buffered.recoveredFiles.size(),size_t{2});
        QCOMPARE(QByteArray(reinterpret_cast<const char*>(buffered.recoveredFiles.at("FRAG.TXT").data()),buffered.recoveredFiles.at("FRAG.TXT").size()),volume.expected());
#endif
    }
    void invalidChainsNamesAndMirrors_data(){QTest::addColumn<QString>("fault");for(const char* value:{"cycle","short","long","free","bad","outside","name","size","crosslink","mirrors","range"})QTest::newRow(value)<<QString(value);}
    void invalidChainsNamesAndMirrors(){
#ifndef _WIN32
        QFETCH(QString,fault);QTemporaryDir root;Volume volume;uint64_t count=volume.sectors;
        if(fault=="cycle")volume.entry(7,2);
        if(fault=="short")volume.entry(2,0xfff);
        if(fault=="long")volume.entry(3,4);
        if(fault=="free")volume.entry(7,0);
        if(fault=="bad")volume.entry(7,0xff7);
        if(fault=="outside")volume.entry(7,4000);
        if(fault=="name")volume.directory(0,"../EVIL TXT",2,2*volume.ss+17);
        if(fault=="size")volume.directory(0,"FRAG    TXT",2,17*1024*1024);
        if(fault=="crosslink")volume.directory(2,"OTHER   TXT",2,2*volume.ss+17);
        if(fault=="mirrors")volume.data[(1+volume.fatSectors)*volume.ss+5]^=1;
        if(fault=="range")count=volume.sectors-1;
        const auto path=root.filePath("source.img");save(path,volume.data);application::service::GUIRecoveryService service(nullptr);const auto source=service.openImage(path.toStdString());
        const auto result=service.executeOperation(operation(source.devicePath,root.path(),0,count));QVERIFY(!result.success);
        if(fault=="mirrors"||fault=="range"){QVERIFY(!result.outputWritten);QVERIFY(!QFile::exists(root.filePath("EMPTY.TXT")));}
        else {QVERIFY(QFile::exists(root.filePath("EMPTY.TXT")));QCOMPARE(result.details.at("Invalid or unsupported root files"),std::string("1"));}
        if(fault!="crosslink")QVERIFY(!QFile::exists(root.filePath("FRAG.TXT")));
#endif
    }
    void guiCliAndCancellation(){
#ifndef _WIN32
        QTemporaryDir root;Volume volume;const auto path=root.filePath("source.img");save(path,volume.data);
        const auto guiOut=root.filePath("gui"),cliOut=root.filePath("cli");QVERIFY(QDir().mkdir(guiOut));QVERIFY(QDir().mkdir(cliOut));
        application::service::GUIRecoveryService service(nullptr);gui::core::MainWindow window(&service);window.show();
        window.findChild<QLineEdit*>("imagePath")->setText(path);window.findChild<QPushButton*>("openImage")->click();window.findChild<QComboBox*>("recoveryMethod")->setCurrentIndex(2);
        window.findChild<QLineEdit*>("recoveryOutput")->setText(guiOut);window.findChild<QPushButton*>("startImageRecovery")->click();
        QTRY_COMPARE_WITH_TIMEOUT(window.findChild<QLabel*>("resultStatus")->text(),QString("Completed"),5000);QTRY_VERIFY(window.findChild<QPushButton*>("recoverySettings")->isEnabled());window.close();
        QProcess process;process.start(QCoreApplication::applicationDirPath()+"/RecoverySuite_CLI",{"image","files",path,"--output",cliOut});QVERIFY(process.waitForFinished(5000));QCOMPARE(process.exitCode(),0);
        QCOMPARE(load(cliOut+"/FRAG.TXT"),load(guiOut+"/FRAG.TXT"));QCOMPARE(load(cliOut+"/FRAG.TXT"),volume.expected());
        const auto source=service.openImage(path.toStdString());bool cancel=false;const auto cancelled=service.executeOperation(operation(source.devicePath,root.path(),0,volume.sectors),
            [&](const auto& progress){if(progress.processedBytes>512+2*9*512+2*512+512)cancel=true;},[&]{return cancel;});
        QVERIFY(cancelled.cancelled);QVERIFY(!cancelled.success);QVERIFY(!cancelled.outputWritten);QVERIFY(!QFile::exists(root.filePath("FRAG.TXT")));
#endif
    }
};
QTEST_MAIN(TestFatFiles)
#include "test_fat_files.moc"
