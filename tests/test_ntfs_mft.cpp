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
        void header(QByteArray& bytes,int flags){bytes.replace(0,4,"FILE");le(bytes,4,48);le(bytes,6,recordBytes/512+1);le(bytes,16,1);le(bytes,20,first);le(bytes,22,flags);le(bytes,28,recordBytes,4);}
        QByteArray protect(QByteArray record)const {
            le(record,48,0xbeef);for(int i=1;i<=recordBytes/512;++i){record[48+2*i]=record[i*512-2];record[48+2*i+1]=record[i*512-1];le(record,i*512-2,0xbeef);}return record;
        }
        QByteArray image()const{QByteArray bytes(sectors*ss,char{0});bytes.replace(0,ss,boot);bytes.replace(8*ss,recordBytes,protect(base));bytes.replace(30*ss,recordBytes,protect(unused));
            bytes.replace(20*ss+recordBytes,recordBytes,QByteArray(recordBytes,char(0x7a)));return bytes;}
        QByteArray expected()const{return protect(base)+protect(unused)+QByteArray(recordBytes,char{0});}
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
