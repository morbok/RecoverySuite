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
    struct Fat32Volume {
        static constexpr int ss=512,sectors=70000,fatSectors=600,dataStart=32+2*fatSectors;
        QByteArray data=QByteArray(sectors*ss,char{0});
        Fat32Volume(){
            le(data,11,ss);data[13]=1;le(data,14,32);data[16]=2;data[21]=char(0xf8);
            le(data,32,sectors,4);le(data,36,fatSectors,4);le(data,44,2,4);data[510]=char(0x55);data[511]=char(0xaa);
            entry(0,0x0ffffff8);entry(1,0x0fffffff);entry(2,10);entry(10,0x0fffffff);
            // A full first root cluster forces traversal to a noncontiguous second cluster.
            for(int i=0;i<16;++i)data[dataStart*ss+i*32]=char(0xe5);
            entry(65538,9);entry(9,4);entry(4,0x0fffffff);
            const int pos=(dataStart+8)*ss;data.replace(pos,11,"HIGH    BIN");data[pos+11]=char(0x20);
            le(data,pos+20,1);le(data,pos+26,2);le(data,pos+28,2*ss+17,4);
            data.replace((dataStart+65536)*ss,ss,QByteArray(ss,'X'));
            data.replace((dataStart+7)*ss,ss,QByteArray(ss,'Y'));data.replace((dataStart+2)*ss,ss,QByteArray(ss,'Z'));
        }
        void entry(int cluster,uint32_t value,int copy=-1){for(int n=0;n<2;++n)if(copy<0||n==copy)le(data,(32+n*fatSectors)*ss+cluster*4,value,4);}
        QByteArray expected()const{return QByteArray(ss,'X')+QByteArray(ss,'Y')+QByteArray(17,'Z');}
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
    void fat32RootChains_data(){
        QTest::addColumn<QString>("variant");
        for(const char* value:{"mirrored","active-second","reserved-high-bits","dirty-flags","root-cycle","root-bad","root-empty","root-crosslink","file-cycle","mirror-mismatch","active-outside","version","reserved-flags","cancel"})
            QTest::newRow(value)<<QString(value);
    }
    void fat32RootChains(){
#ifndef _WIN32
        QFETCH(QString,variant);QTemporaryDir root;Fat32Volume volume;
        if(variant=="active-second"){le(volume.data,40,0x81);volume.entry(65538,0,0);volume.entry(2,0,0);}
        if(variant=="reserved-high-bits"){volume.entry(65538,0xa0000009,0);volume.entry(10,0xffffffff);}
        if(variant=="dirty-flags")volume.entry(1,0x03ffffff);
        if(variant=="root-cycle")volume.entry(10,2);
        if(variant=="root-bad")volume.entry(2,0x0ffffff7);
        if(variant=="root-empty")le(volume.data,44,0x0fffffff,4);
        if(variant=="root-crosslink"){const int pos=(volume.dataStart+8)*512;le(volume.data,pos+20,0);le(volume.data,pos+26,10);le(volume.data,pos+28,512,4);}
        if(variant=="file-cycle")volume.entry(9,65538);
        if(variant=="mirror-mismatch")volume.entry(65538,0,0);
        if(variant=="active-outside")le(volume.data,40,0x82);
        if(variant=="version")le(volume.data,42,1);
        if(variant=="reserved-flags")le(volume.data,40,0x100);
        const auto path=root.filePath("source.img");save(path,volume.data);application::service::GUIRecoveryService service(nullptr);
        const auto source=service.openImage(path.toStdString());bool cancel=false;
        const auto result=service.executeOperation(operation(source.devicePath,root.path(),0,volume.sectors),[&](const auto& progress){
            if(variant=="cancel"&&progress.processedBytes>=512+2*600*512+512)cancel=true;
        },[&]{return cancel;});
        const bool valid=variant=="mirrored"||variant=="active-second"||variant=="reserved-high-bits"||variant=="dirty-flags";
        QCOMPARE(result.success,valid);QCOMPARE(result.outputWritten,valid);
        if(valid){QCOMPARE(result.recoveredItemsCount,uint64_t{1});QCOMPARE(load(root.filePath("HIGH.BIN")),volume.expected());
            QCOMPARE(result.details.at("Deleted entries not reconstructed"),std::string("16"));}
        else QVERIFY(!QFile::exists(root.filePath("HIGH.BIN")));
        if(variant=="cancel")QVERIFY(result.cancelled);
        QCOMPARE(load(path),volume.data);
        if(variant=="active-second"){
            const auto output=root.filePath("cli");QVERIFY(QDir().mkdir(output));QProcess cli;
            cli.start(QCoreApplication::applicationDirPath()+"/RecoverySuite_CLI",{"image","files",path,"--output",output});
            QVERIFY(cli.waitForFinished(5000));QCOMPARE(cli.exitCode(),0);QCOMPARE(load(output+"/HIGH.BIN"),volume.expected());
        }
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
