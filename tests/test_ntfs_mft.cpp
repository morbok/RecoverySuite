#include <QTest>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QProcess>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include "Application/Service/GUIRecoveryService.h"
#include "GUI/core/MainWindow.h"
#include "Recovery/ntfs_mft.h"
#include <map>
#include <limits>
#include <cstring>
#include "Filesystem/NTFS/NTFSStructures.h"
#include "Filesystem/NTFS/NTFSMetadata.h"
#include "Filesystem/NTFS/NTFSAnalyzer.h"
#include "Filesystem/NTFS/NTFSVolume.h"
#include "Filesystem/NTFS/NTFSDriver.h"
using namespace recoverysuite;
class TestNtfsMft:public QObject {
    Q_OBJECT
    static void le(QByteArray& bytes,int pos,uint64_t value,int width=2){for(int i=0;i<width;++i)bytes[pos+i]=char((value>>(8*i))&255);}
    static void save(const QString& path,const QByteArray& bytes){QFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size())throw std::runtime_error("Fixture write failed");}
    static QByteArray load(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("Fixture read failed");return f.readAll();}
    struct Volume {
        int ss,recordBytes,first;static constexpr int sectors=128;
        QByteArray boot,base,unused;
        Volume(int sector=512,bool cross=false):ss(sector),recordBytes(sector==4096?4096:1024),
            first(cross?456:((48+2*(recordBytes/512+1)+7)&~7)),boot(ss,char{0}),base(recordBytes,char{0}),unused(recordBytes,char{0}) {
            boot.replace(3,8,"NTFS    ");le(boot,11,ss);boot[13]=1;le(boot,40,sectors,8);le(boot,48,8,8);boot[64]=char(ss==4096?1:0xf6);le(boot,510,0xaa55);
            header(base,1);header(unused,0);le(unused,first,0xffffffff,4);le(unused,24,first+8,4);
            le(base,24,first+88,4);le(base,first,0x80,4);le(base,first+4,80,4);base[first+8]=1;
            le(base,first+24,4*recordBytes/ss-1,8);le(base,first+32,64);le(base,first+40,4*recordBytes,8);
            le(base,first+48,4*recordBytes,8);le(base,first+56,3*recordBytes,8);
            const char runs[]{0x11,char(recordBytes/ss),8,0x11,char(recordBytes/ss),22,0x11,char(2*recordBytes/ss),char(0xf6),0};
            base.replace(first+64,sizeof(runs),QByteArray(runs,sizeof(runs)));le(base,first+80,0xffffffff,4);
        }
        void header(QByteArray& bytes,int flags)const{bytes.replace(0,4,"FILE");le(bytes,4,48);le(bytes,6,recordBytes/512+1);le(bytes,16,1);le(bytes,20,first);le(bytes,22,flags);le(bytes,28,recordBytes,4);}
        QByteArray protect(QByteArray record)const {
            le(record,48,0xbeef);for(int i=1;i<=recordBytes/512;++i){record[48+2*i]=record[i*512-2];record[48+2*i+1]=record[i*512-1];le(record,i*512-2,0xbeef);}return record;
        }
        QByteArray image()const{QByteArray bytes(sectors*ss,char{0});bytes.replace(0,ss,boot);bytes.replace(8*ss,recordBytes,protect(base));bytes.replace(30*ss,recordBytes,protect(unused));
            bytes.replace(20*ss+recordBytes,recordBytes,QByteArray(recordBytes,char(0x7a)));return bytes;}
        QByteArray expected()const{return protect(base)+protect(unused)+QByteArray(recordBytes,char{0});}
    };
    static uint64_t get(const QByteArray& bytes,int pos,int width=4){uint64_t value=0;for(int i=0;i<width;++i)value|=uint64_t(static_cast<unsigned char>(bytes[pos+i]))<<(8*i);return value;}
    struct ResidentVolume {
        Volume geometry;std::vector<QByteArray> records;QByteArray payload;QString name=QString::fromUtf8("İlk belge 📷.txt");
        ResidentVolume(int sector=512):geometry(sector),records(24,QByteArray(geometry.recordBytes,char{0})),payload(300,char{0}) {
            auto& v=geometry;for(int i=0;i<payload.size();++i)payload[i]=char((i*7)%251);
            const auto clusters=24*v.recordBytes/v.ss;
            le(v.base,v.first+24,clusters-1,8);le(v.base,v.first+40,24*v.recordBytes,8);le(v.base,v.first+48,24*v.recordBytes,8);le(v.base,v.first+56,24*v.recordBytes,8);
            v.base.replace(v.first+64,16,QByteArray(16,char{0}));const uint8_t pairs[]{0x11,uint8_t(v.recordBytes/v.ss),8,0x11,uint8_t(23*v.recordBytes/v.ss),24,0};
            for(size_t i=0;i<sizeof(pairs);++i)v.base[v.first+64+i]=char(pairs[i]);
            records[0]=v.base;
            records[5]=blank(3,7);records[16]=file(name,payload);records[17]=file("EMPTY.TXT",{});
            records[19]=file("DELETED.TXT","deleted");le(records[19],22,0);
            records[20]=blank(3,1);records[21]=file("OUTSIDE.TXT","outside",42|(uint64_t{1}<<48));records[22]=blank(1,1);le(records[22],32,16|(uint64_t{3}<<48),8);
        }
        QByteArray blank(int flags,int sequence)const{QByteArray bytes(geometry.recordBytes,char{0});geometry.header(bytes,flags);le(bytes,16,sequence);le(bytes,24,geometry.first+8,4);le(bytes,geometry.first,0xffffffff,4);return bytes;}
        QByteArray file(const QString& filename,const QByteArray& content,uint64_t parent=5|(uint64_t{7}<<48),bool nonresident=false)const{
            auto bytes=blank(1,3);int pos=geometry.first;
            const auto resident=[&](uint32_t type,const QByteArray& value){const int length=(24+value.size()+7)&~7;
                le(bytes,pos,type,4);le(bytes,pos+4,length,4);le(bytes,pos+16,value.size(),4);le(bytes,pos+20,24);bytes.replace(pos+24,value.size(),value);pos+=length;};
            QByteArray standard(72,char{0});le(standard,32,0x20,4);resident(0x10,standard);
            QByteArray fn(66+filename.size()*2,char{0});le(fn,0,parent,8);le(fn,48,content.size(),8);le(fn,56,0x20,4);fn[64]=char(filename.size());fn[65]=1;
            for(int i=0;i<filename.size();++i)le(fn,66+i*2,filename.utf16()[i]);
            resident(0x30,fn);
            if(nonresident){le(bytes,pos,0x80,4);le(bytes,pos+4,72,4);bytes[pos+8]=1;le(bytes,pos+32,64);le(bytes,pos+40,geometry.ss,8);le(bytes,pos+48,content.size(),8);le(bytes,pos+56,content.size(),8);
                bytes[pos+64]=0x11;bytes[pos+65]=1;bytes[pos+66]=100;pos+=72;
            }else resident(0x80,content);
            le(bytes,pos,0xffffffff,4);le(bytes,24,pos+8,4);return bytes;
        }
        int attribute(const QByteArray& record,unsigned type)const{for(int pos=geometry.first;get(record,pos)!=0xffffffff;pos+=get(record,pos+4))if(get(record,pos)==type)return pos;throw std::runtime_error("Fixture attribute missing");}
        QByteArray image()const{QByteArray bytes(Volume::sectors*geometry.ss,char{0});bytes.replace(0,geometry.ss,geometry.boot);
            for(size_t i=0;i<records.size();++i){const auto offset=i==0?8*geometry.ss:32*geometry.ss+(i-1)*geometry.recordBytes;
                const auto raw=records[i].startsWith("FILE")?geometry.protect(records[i]):records[i];bytes.replace(offset,raw.size(),raw);}
            return bytes;
        }
    };
    static application::service::models::RecoveryOperation operation(const std::string& source,const QString& output,uint64_t start,uint64_t count){
        application::service::models::RecoveryOperation op{};op.operationType="ntfs_mft_snapshot";op.targetDevicePath=source;op.outputPath=output.toStdString();op.startSector=start;op.numSectors=count;op.verifyAfterRecovery=true;return op;
    }
private slots:
    void snapshots_data(){QTest::addColumn<int>("sector");QTest::addColumn<bool>("cross");QTest::newRow("512-fragmented-negative-delta")<<512<<false;QTest::newRow("attribute-crosses-USA-stride")<<512<<true;QTest::newRow("4K-512-byte-fixup-stride")<<4096<<false;}
    void snapshots(){
#ifndef _WIN32
        QFETCH(int,sector);QFETCH(bool,cross);QTemporaryDir root;Volume volume(sector,cross);const auto image=QByteArray(3*sector,char(0x33))+volume.image();const auto path=root.filePath("source.img");save(path,image);
        application::service::GUIRecoveryService service(nullptr);const auto source=service.openImage(path.toStdString(),sector);auto op=operation(source.devicePath,root.path(),3,volume.sectors);
        const auto result=service.executeOperation(op);QVERIFY2(result.success,result.errorMessage.c_str());QCOMPARE(result.recoveredItemsCount,uint64_t{2});QCOMPARE(result.recoveredBytes,uint64_t(sector+3*volume.recordBytes));
        QCOMPARE(load(root.filePath("NTFS_BootSector")),volume.boot);QCOMPARE(load(root.filePath("NTFS_MFTInitialized")),volume.expected());QCOMPARE(load(path),image);
        QCOMPARE(result.details.at("MFT in-use record flags"),std::string("1"));QCOMPARE(result.details.at("MFT unused record flags"),std::string("1"));QCOMPARE(result.details.at("MFT zero-filled record slots"),std::string("1"));
        const auto again=service.executeOperation(op);QVERIFY(!again.success);QCOMPARE(again.failedItemsCount,uint64_t{2});QCOMPARE(load(root.filePath("NTFS_MFTInitialized")),volume.expected());
        const auto out=root.filePath("cli");QVERIFY(QDir().mkdir(out));QProcess cli;cli.start(QCoreApplication::applicationDirPath()+"/RecoverySuite_CLI",{"image","mft",path,"--start","3","--count",QString::number(volume.sectors),"--sector-size",QString::number(sector),"--output",out});
        QVERIFY(cli.waitForFinished(5000));QCOMPARE(cli.exitCode(),0);QCOMPARE(load(out+"/NTFS_MFTInitialized"),volume.expected());
        if(sector==512&&!cross){
            const auto guiOut=root.filePath("gui");QVERIFY(QDir().mkdir(guiOut));gui::core::MainWindow window(&service);window.show();
            window.findChild<QLineEdit*>("imagePath")->setText(path);window.findChild<QPushButton*>("openImage")->click();window.findChild<QComboBox*>("recoveryMethod")->setCurrentIndex(4);
            window.findChild<QLineEdit*>("recoveryStart")->setText("3");window.findChild<QLineEdit*>("recoveryCount")->setText(QString::number(volume.sectors));window.findChild<QLineEdit*>("recoveryOutput")->setText(guiOut);
            window.findChild<QPushButton*>("startImageRecovery")->click();QTRY_COMPARE_WITH_TIMEOUT(window.findChild<QLabel*>("resultStatus")->text(),QString("Completed"),5000);
            QTRY_VERIFY(window.findChild<QPushButton*>("recoverySettings")->isEnabled());window.close();QCOMPARE(load(guiOut+"/NTFS_MFTInitialized"),volume.expected());
        }
#endif
    }
    void residentFiles_data(){QTest::addColumn<int>("sector");QTest::newRow("resident-512")<<512;QTest::newRow("resident-4096")<<4096;}
    void residentFiles(){
#ifndef _WIN32
        QFETCH(int,sector);QTemporaryDir root;ResidentVolume v(sector);const auto original=QByteArray(3*sector,char(0x44))+v.image();const auto path=root.filePath("source.img");save(path,original);
        application::service::GUIRecoveryService service(nullptr);const auto source=service.openImage(path.toStdString(),sector);auto op=operation(source.devicePath,root.path(),3,Volume::sectors);op.operationType="ntfs_resident_recovery";
        const auto result=service.executeOperation(op);QVERIFY2(result.success,result.errorMessage.c_str());QCOMPARE(result.recoveredItemsCount,uint64_t{2});QCOMPARE(result.recoveredBytes,uint64_t(v.payload.size()));
        QCOMPARE(load(root.filePath(v.name)),v.payload);QCOMPARE(load(root.filePath("EMPTY.TXT")),QByteArray());QCOMPARE(load(path),original);
        QCOMPARE(result.details.at("Unused user-record slots skipped"),std::string("1"));QCOMPARE(result.details.at("Directory records skipped"),std::string("1"));QCOMPARE(result.details.at("Non-root files skipped"),std::string("1"));QCOMPARE(result.details.at("Extension records skipped"),std::string("1"));
        const auto repeated=service.executeOperation(op);QVERIFY(!repeated.success);QCOMPARE(repeated.failedItemsCount,uint64_t{2});QCOMPARE(load(root.filePath(v.name)),v.payload);
        const auto out=root.filePath("cli");QVERIFY(QDir().mkdir(out));QProcess cli;cli.start(QCoreApplication::applicationDirPath()+"/RecoverySuite_CLI",{"image","ntfs-files",path,"--start","3","--sector-size",QString::number(sector),"--output",out});
        QVERIFY(cli.waitForFinished(5000));QCOMPARE(cli.exitCode(),0);QCOMPARE(load(out+"/"+v.name),v.payload);
        if(sector==512){const auto guiOut=root.filePath("gui");QVERIFY(QDir().mkdir(guiOut));gui::core::MainWindow window(&service);window.show();
            window.findChild<QLineEdit*>("imagePath")->setText(path);window.findChild<QPushButton*>("openImage")->click();window.findChild<QComboBox*>("recoveryMethod")->setCurrentIndex(5);
            window.findChild<QLineEdit*>("recoveryStart")->setText("3");window.findChild<QLineEdit*>("recoveryCount")->setText(QString::number(Volume::sectors));window.findChild<QLineEdit*>("recoveryOutput")->setText(guiOut);
            window.findChild<QPushButton*>("startImageRecovery")->click();QTRY_COMPARE_WITH_TIMEOUT(window.findChild<QLabel*>("resultStatus")->text(),QString("Completed"),5000);
            QTRY_VERIFY(window.findChild<QPushButton*>("recoverySettings")->isEnabled());window.close();QCOMPARE(load(guiOut+"/"+v.name),v.payload);}
#endif
    }
    void residentFaults_data(){QTest::addColumn<QString>("fault");for(const char* f:{"stale-parent","unsafe-name","bad-unicode","name-length","namespace","data-offset","data-length","missing-data","missing-standard","duplicate-data","attribute-end","nonresident","compressed","encrypted","reparse","attribute-list","directory-flag"})QTest::newRow(f)<<QString(f);}
    void residentFaults(){
#ifndef _WIN32
        QFETCH(QString,fault);QTemporaryDir root;ResidentVolume v;auto& record=v.records[16];const int data=v.attribute(record,0x80),fn=v.attribute(record,0x30),standard=v.attribute(record,0x10);const int value=fn+24;
        if(fault=="stale-parent")le(record,value,5|(uint64_t{8}<<48),8);
        if(fault=="unsafe-name")le(record,value+66,0x2f);
        if(fault=="bad-unicode")le(record,value+66,0xd800);
        if(fault=="name-length")record[value+64]=char(255);
        if(fault=="namespace")record[value+65]=4;
        if(fault=="data-offset")le(record,data+20,1020);
        if(fault=="data-length")le(record,data+16,1024,4);
        if(fault=="missing-data")le(record,data,0x40,4);
        if(fault=="missing-standard")le(record,standard,0x40,4);
        if(fault=="duplicate-data"){const int end=get(record,24)-8,length=get(record,data+4);record.replace(end,length,record.mid(data,length));le(record,end+length,0xffffffff,4);le(record,24,end+length+8,4);}
        if(fault=="attribute-end")le(record,get(record,24)-8,0,4);
        if(fault=="nonresident")record=v.file(v.name,v.payload,5|(uint64_t{7}<<48),true);
        if(fault=="compressed")le(record,data+12,1);
        if(fault=="encrypted")le(record,standard+24+32,0x4000,4);
        if(fault=="reparse")le(record,standard+24+32,0x400,4);
        if(fault=="attribute-list")le(record,standard,0x20,4);
        if(fault=="directory-flag")le(record,standard+24+32,0x10,4);
        const auto path=root.filePath("source.img");const auto image=v.image();save(path,image);application::service::GUIRecoveryService service(nullptr);const auto source=service.openImage(path.toStdString());auto op=operation(source.devicePath,root.path(),0,Volume::sectors);op.operationType="ntfs_resident_recovery";
        const auto result=service.executeOperation(op);QVERIFY(!result.success);QCOMPARE(result.recoveredItemsCount,uint64_t{1});QVERIFY(result.outputWritten);QCOMPARE(load(root.filePath("EMPTY.TXT")),QByteArray());QVERIFY(!QFile::exists(root.filePath(v.name)));QCOMPARE(load(path),image);
        const auto key=fault=="nonresident"?"Nonresident NTFS root files not reconstructed":(fault=="compressed"||fault=="encrypted"||fault=="reparse"||fault=="attribute-list"?"Unsupported NTFS root-file features":"Invalid NTFS root files");
        QCOMPARE(result.details.at(key),std::string("1"));
#endif
    }
    void rootValidationAliasesAndCancellation(){
#ifndef _WIN32
        QTemporaryDir root;ResidentVolume v;auto& record=v.records[16];const int before=v.attribute(record,0x30),used=get(record,24);const auto aliasRecord=v.file("ILKBEL~1.TXT",v.payload);const int aliasPos=v.attribute(aliasRecord,0x30),aliasLength=get(aliasRecord,aliasPos+4);
        auto alias=aliasRecord.mid(aliasPos,aliasLength);alias[24+65]=2;const auto tail=record.mid(before,used-before);record.replace(before+aliasLength,tail.size(),tail);record.replace(before,aliasLength,alias);le(record,24,used+aliasLength,4);
        const auto path=root.filePath("source.img");save(path,v.image());application::service::GUIRecoveryService service(nullptr);auto source=service.openImage(path.toStdString());auto op=operation(source.devicePath,root.path(),0,Volume::sectors);op.operationType="ntfs_resident_recovery";
        const auto result=service.executeOperation(op);QVERIFY2(result.success,result.errorMessage.c_str());QCOMPARE(load(root.filePath(v.name)),v.payload);QVERIFY(!QFile::exists(root.filePath("ILKBEL~1.TXT")));QCOMPARE(result.details.at("Other root names not exported"),std::string("1"));
        const auto out=root.filePath("cancel");QVERIFY(QDir().mkdir(out));op.outputPath=out.toStdString();bool cancel=false;
        const auto stopped=service.executeOperation(op,[&](const auto& p){if(p.processedBytes>=512+1024+24*1024)cancel=true;},[&]{return cancel;});QVERIFY(stopped.cancelled);QVERIFY(!stopped.outputWritten);
        le(v.records[5],22,1);save(path,v.image());source=service.openImage(path.toStdString());op.targetDevicePath=source.devicePath;const auto bad=service.executeOperation(op);QVERIFY(!bad.success);QVERIFY(!bad.outputWritten);QVERIFY(QDir(out).entryList(QDir::NoDotAndDotDot|QDir::AllEntries).isEmpty());
#endif
    }
    void corruption_data(){QTest::addColumn<QString>("fault");for(const char* f:{"oem","sector","cluster-zero","cluster-power","volume","range","mft","record-zero","record-exponent","signature","usa-offset","usa-count","usa-tag","late-usa","allocated-record","used-record","first-attribute","attribute-size","attribute-form","attribute-name","attribute-list","missing-data","missing-end","data-resident","data-flags","start-vcn","high-vcn","mapping-offset","mapping-zero","mapping-width","mapping-sparse","mapping-large","mapping-negative","mapping-overlap","mapping-first","mapping-end","initialized-large","initialized-align","initialized-size","logical-align","allocation-size"})QTest::newRow(f)<<QString(f);}
    void corruption(){
#ifndef _WIN32
        QFETCH(QString,fault);QTemporaryDir root;Volume v;const auto a=v.first;uint64_t count=v.sectors;
        if(fault=="oem")v.boot[3]=char(0x58);
        if(fault=="sector")le(v.boot,11,4096);
        if(fault=="cluster-zero")v.boot[13]=0;
        if(fault=="cluster-power")v.boot[13]=3;
        if(fault=="volume")le(v.boot,40,1000,8);
        if(fault=="range")--count;
        if(fault=="mft")le(v.boot,48,128,8);
        if(fault=="record-zero")v.boot[64]=0;
        if(fault=="record-exponent")v.boot[64]=char(0x80);
        if(fault=="signature")v.base[0]=char(0x58);
        if(fault=="usa-offset")le(v.base,4,510);
        if(fault=="usa-count")le(v.base,6,2);
        if(fault=="allocated-record")le(v.base,28,512,4);
        if(fault=="used-record")le(v.base,24,2048,4);
        if(fault=="first-attribute")le(v.base,20,57);
        if(fault=="attribute-size")le(v.base,a+4,0,4);
        if(fault=="attribute-form")v.base[a+8]=2;
        if(fault=="attribute-name"){v.base[a+9]=5;le(v.base,a+10,78);}
        if(fault=="attribute-list")le(v.base,a,0x20,4);
        if(fault=="missing-data")le(v.base,a,0x10,4);
        if(fault=="missing-end")le(v.base,a+80,0,4);
        if(fault=="data-resident")v.base[a+8]=0;
        if(fault=="data-flags")le(v.base,a+12,0x8000);
        if(fault=="start-vcn")le(v.base,a+16,1,8);
        if(fault=="high-vcn")le(v.base,a+24,1,8);
        if(fault=="mapping-offset")le(v.base,a+32,79);
        if(fault=="mapping-zero")v.base[a+65]=0;
        if(fault=="mapping-width")v.base[a+64]=char(0x19);
        if(fault=="mapping-sparse")v.base[a+64]=1;
        if(fault=="mapping-large")v.base[a+65]=char(127);
        if(fault=="mapping-negative")v.base[a+66]=char(0xff);
        if(fault=="mapping-overlap")v.base[a+69]=0;
        if(fault=="mapping-first")v.base[a+66]=7;
        if(fault=="mapping-end")v.base.replace(a+73,7,QByteArray(7,char(0x11)));
        if(fault=="initialized-large")le(v.base,a+56,16*1024*1024+1024,8);
        if(fault=="initialized-align")le(v.base,a+56,1025,8);
        if(fault=="initialized-size")le(v.base,a+56,8192,8);
        if(fault=="logical-align")le(v.base,a+48,4095,8);
        if(fault=="allocation-size")le(v.base,a+40,8192,8);
        auto image=v.image();
        if(fault=="usa-tag")image[8*512+510]^=1;
        if(fault=="late-usa")image[30*512+1022]^=1;
        const auto path=root.filePath("source.img");save(path,image);application::service::GUIRecoveryService service(nullptr);const auto source=service.openImage(path.toStdString());
        const auto result=service.executeOperation(operation(source.devicePath,root.path(),0,count));QVERIFY2(!result.success,qPrintable(fault));QVERIFY(!result.outputWritten);
        QVERIFY(!QFile::exists(root.filePath("NTFS_BootSector")));QVERIFY(!QFile::exists(root.filePath("NTFS_MFTInitialized")));QCOMPARE(load(path),image);
#endif
    }
    void legacyMetadataUsesRealBootFields(){
        using namespace filesystem::ntfs;
        class Reader:public filesystem::FilesystemReader {
        public:
            QByteArray boot;uint32_t ss=512;uint64_t total=0x100000080,offset=1024;bool shortRead=false;
            uint32_t readSector(uint64_t i,uint8_t* out,uint32_t size)override{if(i||!out||size!=ss||boot.size()<size)return 0;std::memcpy(out,boot.constData(),size);return shortRead?size-1:size;}
            uint32_t readSectors(uint64_t,uint32_t,uint8_t*,uint32_t)override{return 0;}
            uint32_t readCluster(uint64_t,uint8_t*,uint32_t)override{return 0;}
            uint32_t readClusters(uint64_t,uint32_t,uint8_t*,uint32_t)override{return 0;}
            uint64_t getTotalSectors()const override{return total;}uint32_t getSectorSize()const override{return ss;}uint64_t getStartOffset()const override{return offset;}
        };
        QCOMPARE(sizeof(BootSector),size_t{512});QCOMPARE(offsetof(BootSector,totalSectorCount64),size_t{40});QCOMPARE(offsetof(BootSector,mftCluster),size_t{48});QCOMPARE(offsetof(BootSector,bootSignature),size_t{510});
        Volume fixture;auto reader=std::make_shared<Reader>();reader->boot=fixture.boot;le(reader->boot,40,reader->total,8);le(reader->boot,48,0x100000008,8);le(reader->boot,72,0x0123456789abcdef,8);
        NTFSMetadata metadata(reader);NTFSVolume volume(reader);NTFSAnalyzer analyzer(reader);NTFSDriver driver;
        QCOMPARE(metadata.getTotalSize(),reader->total*512);QCOMPARE(metadata.getSerialNumber(),std::string("0123456789ABCDEF"));QCOMPARE(metadata.getVersion(),std::string("Unknown"));
        QVERIFY(volume.isValid());QCOMPARE(volume.getSize(),reader->total*512);QCOMPARE(volume.getStartOffset(),uint64_t{1024});QCOMPARE(volume.getEndOffset(),1024+reader->total*512);
        QCOMPARE(driver.probe(reinterpret_cast<const uint8_t*>(reader->boot.constData()),reader->boot.size()),1.0);QVERIFY(!analyzer.analyze(*reader));
        QVERIFY(QString::fromStdString(analyzer.getAnalysisDetails()).contains("unknown",Qt::CaseInsensitive));QVERIFY_EXCEPTION_THROWN(analyzer.needsRepair(),std::logic_error);QVERIFY_EXCEPTION_THROWN(analyzer.getErrorCount(),std::logic_error);
        QVERIFY_EXCEPTION_THROWN(metadata.getFreeSpace(),std::logic_error);QVERIFY_EXCEPTION_THROWN(metadata.getUsedSpace(),std::logic_error);QVERIFY_EXCEPTION_THROWN(metadata.getFreeBlockCount(),std::logic_error);QVERIFY_EXCEPTION_THROWN(metadata.isMounted(),std::logic_error);QVERIFY(metadata.isReadOnly());
        QVERIFY_EXCEPTION_THROWN(volume.isBootable(),std::logic_error);QVERIFY_EXCEPTION_THROWN(volume.isActive(),std::logic_error);QVERIFY_EXCEPTION_THROWN(volume.isHidden(),std::logic_error);
        reader->boot=fixture.boot;reader->total=Volume::sectors;reader->boot[13]=8;QCOMPARE(metadata.getBlockSize(),uint32_t{4096});QCOMPARE(metadata.getBlockCount(),uint64_t{16});
        reader->offset=std::numeric_limits<uint64_t>::max()-100;QCOMPARE(volume.getEndOffset(),uint64_t{0});
        reader->shortRead=true;QVERIFY(!volume.isValid());QCOMPARE(metadata.getTotalSize(),uint64_t{0});reader->shortRead=false;
        reader->boot.replace(3,8,"NTFSjunk");QVERIFY(!volume.isValid());QCOMPARE(driver.probe(reinterpret_cast<const uint8_t*>(reader->boot.constData()),reader->boot.size()),0.0);
        reader->ss=256;QCOMPARE(metadata.getTotalSize(),uint64_t{0});QVERIFY(!volume.isValid());
    }
    void hugeGeometryAndFaultReads(){
        class SparseReader:public disk::IDiskReader {
        public:
            std::map<uint64_t,QByteArray> blocks;uint64_t total{};int fault{},calls{},writes{};bool outside{};
            void open(const std::string&)override{}void close()override{}bool isOpen()const noexcept override{return true;}
            uint32_t getSectorSize()const noexcept override{return 512;}uint64_t getTotalSectors()const noexcept override{return total;}
            disk::DeviceInformation getDiskInfo()const override{return {};}
            bool writeSectors(uint64_t,uint64_t,const std::vector<std::byte>&)override{++writes;return false;}
            bool readSectors(uint64_t start,uint64_t count,std::vector<std::byte>& out)override{
                ++calls;if(start>=total||count>total-start){outside=true;return false;}out.assign(count*512,std::byte{0});
                for(uint64_t i=0;i<count;++i)for(const auto& [first,bytes]:blocks)if(start+i>=first&&start+i-first<uint64_t(bytes.size()/512)){
                    for(size_t j=0;j<512;++j)out[i*512+j]=std::byte(static_cast<uint8_t>(bytes[(start+i-first)*512+j]));
                }
                if(calls==2){if(fault==1)out.pop_back();if(fault==2)out.push_back(std::byte{0});if(fault==3)return false;}return true;
            }
        };
        Volume volume;constexpr uint64_t partition=17,mft=0x100000008,total=0x100000080;
        le(volume.boot,40,total,8);le(volume.boot,48,mft,8);
        const uint8_t pairs[]{0x51,2,8,0,0,0,1,0x11,2,22,0x11,4,0xf6,0};
        volume.base.replace(volume.first+64,16,QByteArray(16,char{0}));
        for(size_t i=0;i<sizeof(pairs);++i)volume.base[volume.first+64+i]=char(pairs[i]);
        for(int fault=0;fault<4;++fault){
            SparseReader reader;reader.total=partition+total;reader.fault=fault;
            reader.blocks={{partition,volume.boot},{partition+mft,volume.protect(volume.base)},{partition+mft+22,volume.protect(volume.unused)}};
            if(fault)QVERIFY_EXCEPTION_THROWN(recovery::readNtfsMft(reader,partition,total),std::runtime_error);
            else {const auto snapshot=recovery::readNtfsMft(reader,partition,total);QCOMPARE(snapshot.inUseRecords,uint64_t{1});QCOMPARE(snapshot.initializedRecords.size(),size_t{3072});
                QCOMPARE(QByteArray(reinterpret_cast<const char*>(snapshot.initializedRecords.data()),snapshot.initializedRecords.size()),volume.expected());}
            QVERIFY(!reader.outside);QCOMPARE(reader.writes,0);
        }
    }
    void cancellationAndPartialOutput(){
#ifndef _WIN32
        QTemporaryDir root;Volume v;const auto path=root.filePath("source.img");save(path,v.image());application::service::GUIRecoveryService service(nullptr);const auto source=service.openImage(path.toStdString());
        auto op=operation(source.devicePath,root.path(),0,v.sectors);bool cancel=false;const auto stopped=service.executeOperation(op,[&](const auto& p){if(p.processedBytes>=uint64_t(512+2*v.recordBytes))cancel=true;},[&]{return cancel;});
        QVERIFY(stopped.cancelled);QVERIFY(!stopped.outputWritten);QVERIFY(!QFile::exists(root.filePath("NTFS_BootSector")));
        save(root.filePath("NTFS_MFTInitialized"),"PRESERVE");const auto partial=service.executeOperation(op);QVERIFY(!partial.success);QVERIFY(partial.outputWritten);QCOMPARE(partial.recoveredItemsCount,uint64_t{1});QCOMPARE(partial.failedItemsCount,uint64_t{1});
        QCOMPARE(load(root.filePath("NTFS_BootSector")),v.boot);QCOMPARE(load(root.filePath("NTFS_MFTInitialized")),QByteArray("PRESERVE"));QCOMPARE(load(path),v.image());
#endif
    }
};
QTEST_MAIN(TestNtfsMft)
#include "test_ntfs_mft.moc"
