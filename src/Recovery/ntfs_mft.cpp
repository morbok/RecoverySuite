#include "ntfs_mft.h"
#include "source_bytes.h"
#include "unicode_file_name.h"
#include <set>
#include <algorithm>
#include <limits>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
namespace recoverysuite::recovery {
namespace {
constexpr uint64_t kMaxSnapshotBytes=16*1024*1024;
constexpr uint64_t kSequenceStride=512;
constexpr uint64_t kMaxRecordBytes=65536;
constexpr size_t kMaxRuns=4096;
using Bytes=std::span<const uint8_t>;
[[noreturn]] void invalid(const char* message){throw std::runtime_error(std::string("NTFS MFT: ")+message);}
uint64_t le(Bytes bytes,size_t pos,size_t width) {
    if(!width||width>8||pos>bytes.size()||width>bytes.size()-pos)invalid("truncated metadata field");
    uint64_t value=0;for(size_t i=0;i<width;++i)value|=uint64_t{bytes[pos+i]}<<(8*i);return value;
}
std::vector<uint8_t> decodeRecord(Bytes raw) {
    if(raw.size()<kSequenceStride||raw.size()%kSequenceStride||!std::equal(raw.begin(),raw.begin()+4,"FILE"))invalid("invalid FILE record signature or size");
    const auto usa=le(raw,4,2),words=le(raw,6,2),first=le(raw,20,2),used=le(raw,24,4);
    if(usa<42||usa%2||words!=raw.size()/kSequenceStride+1||usa+words*2>kSequenceStride-2||
       first<48||first%8||first<usa+words*2||first>used||used-first<4||used>raw.size()||
       le(raw,28,4)!=raw.size()||(le(raw,22,2)&~uint64_t{3}))invalid("invalid FILE header or update sequence array");
    const auto sequence=le(raw,usa,2);
    for(size_t i=1;i<words;++i)if(le(raw,i*kSequenceStride-2,2)!=sequence)invalid("torn FILE record (update sequence mismatch)");
    std::vector<uint8_t> decoded(raw.begin(),raw.end());
    for(size_t i=1;i<words;++i){decoded[i*kSequenceStride-2]=raw[usa+2*i];decoded[i*kSequenceStride-1]=raw[usa+2*i+1];}
    return decoded;
}
struct Run {uint64_t lcn,clusters;};
struct Stream {std::vector<Run> runs;uint64_t initialized{},logical{},allocated{};};
struct Attribute {uint64_t type;Bytes bytes;bool nonResident;uint64_t nameLength;};
std::vector<Attribute> listAttributes(Bytes record,const std::function<bool()>& cancelled) {
    std::vector<Attribute> result;const auto used=le(record,24,4);
    for(size_t pos=le(record,20,2);pos+4<=used;) {
        if(cancelled&&cancelled())invalid("cancelled");
        const auto type=le(record,pos,4);if(type==0xffffffff)return result;
        if(used-pos<16)invalid("truncated attribute header");
        const auto length=le(record,pos+4,4),form=le(record,pos+8,1),nameLength=le(record,pos+9,1),nameOffset=le(record,pos+10,2);
        const uint64_t header=form==0?24:64;
        if(form>1||length<header||length%8||length>used-pos)invalid("invalid attribute length or form");
        const auto attribute=record.subspan(pos,length);
        if(nameLength&&(nameOffset<header||nameOffset>length||nameLength*2>length-nameOffset))invalid("invalid attribute name bounds");
        if(form==0) {
            const auto offset=le(attribute,20,2),size=le(attribute,16,4);
            if(offset<header||offset>length||size>length-offset||(nameLength&&nameOffset+nameLength*2>offset))invalid("invalid resident value bounds");
        }
        result.push_back({type,attribute,form==1,nameLength});pos+=length;
    }
    invalid("missing attribute end marker");
}
Stream parseData(Bytes attribute,uint64_t clusters,uint64_t clusterBytes,const std::function<bool()>& cancelled,bool mft=false) {
    Stream stream;const auto length=attribute.size();constexpr uint64_t header=64;
    if(length<header||le(attribute,12,2)||le(attribute,16,8)||le(attribute,34,2))invalid("unsupported DATA flags or extension");
    stream.allocated=le(attribute,40,8);stream.logical=le(attribute,48,8);stream.initialized=le(attribute,56,8);
    if(stream.initialized>kMaxSnapshotBytes||(!mft&&stream.logical>kMaxSnapshotBytes)||stream.initialized>stream.logical||stream.logical>stream.allocated)invalid("invalid DATA sizes or stream above 16 MiB");
    size_t cursor=le(attribute,32,2);
    if(cursor<header||cursor>=length)invalid("invalid mapping-pairs offset");
    uint64_t current=0,total=0;bool terminated=false;std::map<uint64_t,uint64_t> physical;
    while(cursor<length) {
        if(cancelled&&cancelled())invalid("cancelled");
        const auto code=attribute[cursor++];if(!code){terminated=true;break;}
        const size_t lengths=code&15,offsets=code>>4;
        if(!lengths||lengths>8||!offsets||offsets>8||stream.runs.size()>=kMaxRuns)invalid("invalid, sparse or excessive mapping pairs");
        const auto runLength=le(attribute,cursor,lengths);cursor+=lengths;
        const auto delta=le(attribute,cursor,offsets);cursor+=offsets;
        if(!runLength||runLength>clusters-total)invalid("invalid mapping-pairs run length");
        if(delta&(uint64_t{1}<<(offsets*8-1))) {
            const auto mask=offsets==8?std::numeric_limits<uint64_t>::max():((uint64_t{1}<<(offsets*8))-1);
            const auto magnitude=(~delta+1)&mask;
            if(magnitude>current)invalid("negative mapping-pairs LCN");
            current-=magnitude;
        }else {if(delta>clusters-current)invalid("mapping-pairs LCN overflow");current+=delta;}
        if(!current||current>=clusters||runLength>clusters-current)invalid("mapping pairs exceed the declared volume");
        const auto next=physical.lower_bound(current);
        if((next!=physical.end()&&current+runLength>next->first)||
           (next!=physical.begin()&&std::prev(next)->second>current))invalid("overlapping DATA runs");
        physical.emplace(current,current+runLength);stream.runs.push_back({current,runLength});total+=runLength;
    }
    if(!terminated||stream.runs.empty()||le(attribute,24,8)!=total-1||stream.allocated!=total*clusterBytes)
        invalid("incomplete or inconsistent DATA mapping pairs");
    return stream;
}
Stream parseMftData(Bytes record,uint64_t clusters,uint64_t clusterBytes,uint64_t mftCluster,const std::function<bool()>& cancelled) {
    Stream stream;bool found=false;
    for(const auto& entry:listAttributes(record,cancelled)) {
        if(entry.type==0x20)invalid("MFT attribute-list extensions are unsupported");
        if(entry.type!=0x80||entry.nameLength)continue;
        if(found||!entry.nonResident)invalid("duplicate or resident MFT DATA");
        found=true;stream=parseData(entry.bytes,clusters,clusterBytes,cancelled,true);
    }
    if(!found||!stream.initialized||stream.runs.front().lcn!=mftCluster)invalid("missing or invalid MFT DATA");
    return stream;
}
}
NtfsMftSnapshot readNtfsMft(disk::IDiskReader& reader,uint64_t start,uint64_t count,
    const std::function<void(uint64_t)>& progress,const std::function<bool()>& cancelled) {
    NtfsMftSnapshot result;
    const auto check=[&]{if(cancelled&&cancelled())invalid("cancelled");};
    uint64_t volumeBytes=std::numeric_limits<uint64_t>::max();
    const auto read=[&](uint64_t offset,uint64_t length) {
        check();std::vector<uint8_t> bytes;
        if(offset>volumeBytes||length>volumeBytes-offset||!readSourceBytes(&reader,start,count,offset,length,bytes,cancelled)) {
            check();invalid("source read failed or exceeded the volume/range");
        }
        result.processedBytes+=length;if(progress)progress(result.processedBytes);return bytes;
    };
    result.bootSector=read(0,reader.getSectorSize());const Bytes boot=result.bootSector;
    if(boot.size()<512||!std::equal(boot.begin()+3,boot.begin()+11,"NTFS    ")||le(boot,510,2)!=0xaa55)invalid("invalid NTFS boot sector");
    const auto sector=le(boot,11,2),spc=le(boot,13,1),total=le(boot,40,8),mft=le(boot,48,8);
    if(sector!=reader.getSectorSize()||sector<512||sector>4096||(sector&(sector-1))||!spc||(spc&(spc-1))||spc>128||
       !total||total>count||total>std::numeric_limits<uint64_t>::max()/sector)invalid("invalid or truncated NTFS geometry");
    volumeBytes=total*sector;const auto clusterBytes=sector*spc,clusters=total/spc;
    if(!mft||mft>=clusters)invalid("MFT cluster is outside the declared volume");
    const auto encoded=le(boot,64,1);
    if(encoded>=128) {
        const auto exponent=256-encoded;if(exponent<9||exponent>16)invalid("unsupported record-size exponent");result.recordSize=uint64_t{1}<<exponent;
    }else result.recordSize=encoded*clusterBytes;
    if(result.recordSize<kSequenceStride||result.recordSize>kMaxRecordBytes||(result.recordSize&(result.recordSize-1)))invalid("unsupported FILE record size");
    const auto bootstrap=read(mft*clusterBytes,result.recordSize);const auto decoded=decodeRecord(bootstrap);
    if(le(decoded,22,2)!=1||le(decoded,32,8)||!le(decoded,16,2))invalid("invalid MFT base record");
    const auto stream=parseMftData(decoded,clusters,clusterBytes,mft,cancelled);
    if(stream.runs.front().clusters*clusterBytes<result.recordSize||stream.initialized%result.recordSize||stream.logical%result.recordSize||stream.initialized<result.recordSize)
        invalid("fragmented bootstrap record or incomplete FILE record lengths");
    result.initializedRecords.reserve(static_cast<size_t>(stream.initialized));
    for(const auto& run:stream.runs) {
        check();const auto remaining=stream.initialized-result.initializedRecords.size();if(!remaining)break;
        const auto bytes=read(run.lcn*clusterBytes,std::min(remaining,run.clusters*clusterBytes));
        result.initializedRecords.insert(result.initializedRecords.end(),bytes.begin(),bytes.end());
    }
    if(result.initializedRecords.size()!=stream.initialized||!std::equal(bootstrap.begin(),bootstrap.end(),result.initializedRecords.begin()))invalid("MFT bootstrap changed or stream is incomplete");
    for(size_t pos=0;pos<result.initializedRecords.size();pos+=result.recordSize) {
        check();const auto raw=Bytes(result.initializedRecords).subspan(pos,result.recordSize);
        if(std::all_of(raw.begin(),raw.end(),[](uint8_t c){return c==0;})){++result.blankRecords;continue;}
        const auto record=decodeRecord(raw);
        if(le(record,22,2)&1)++result.inUseRecords;else ++result.unusedRecords;
    }
    check();return result;
}
static NtfsResidentSummary recoverRootFiles(disk::IDiskReader& reader,uint64_t start,uint64_t count,
    const std::function<void(const std::string&,const std::vector<uint8_t>&)>& item,
    const std::function<void(uint64_t)>& progress,const std::function<bool()>& cancelled,bool includeNonresident) {
    constexpr size_t kRootRecord=5,kFirstUserRecord=16,kMaxOutputs=10000;
    const auto check=[&]{if(cancelled&&cancelled())invalid("cancelled");};
    const auto mft=readNtfsMft(reader,start,count,progress,cancelled);NtfsResidentSummary result;result.processedBytes=mft.processedBytes;
    const Bytes boot=mft.bootSector;
    const auto clusterBytes=le(boot,11,2)*le(boot,13,1),clusters=le(boot,40,8)/le(boot,13,1);
    const Bytes records=mft.initializedRecords;
    const auto reserved=parseMftData(decodeRecord(records.first(mft.recordSize)),clusters,clusterBytes,le(boot,48,8),cancelled);
    std::map<uint64_t,uint64_t> claimed;
    for(const auto& run:reserved.runs)claimed.emplace(run.lcn,run.lcn+run.clusters);
    const auto slots=records.size()/mft.recordSize;
    if(slots<=kRootRecord)invalid("root directory record is not initialized");
    const auto root=decodeRecord(records.subspan(kRootRecord*mft.recordSize,mft.recordSize));
    const auto rootSequence=le(root,16,2);
    if(le(root,22,2)!=3||le(root,32,8)||!rootSequence)invalid("invalid root directory base record");
    for(size_t index=kFirstUserRecord;index<slots;++index) {
        check();const auto raw=records.subspan(index*mft.recordSize,mft.recordSize);
        if(std::all_of(raw.begin(),raw.end(),[](uint8_t c){return c==0;}))continue;
        const auto record=decodeRecord(raw);const auto flags=le(record,22,2);
        if(!(flags&1)){++result.unusedRecords;continue;}
        if(flags&2){++result.directoryRecords;continue;}
        if(le(record,32,8)){++result.extensionRecords;continue;}
        if(result.foundItems+result.invalidFiles+result.nonresidentFiles+result.unsupportedFiles>=kMaxOutputs)invalid("NTFS output item limit reached");
        std::string selected;std::vector<uint8_t> data;bool recoveredNonresident=false;
        try {
            if(!le(record,16,2))invalid("zero user-record sequence");
            bool standard=false,hasData=false,nonresident=false,unsupported=false,staleRoot=false,filename=false,rootLink=false;
            Bytes external;int preferred=-1;std::set<std::string> names;
            for(const auto& attribute:listAttributes(record,cancelled)) {
                const auto bytes=attribute.bytes;
                if(attribute.type==0x20||attribute.type==0xc0)unsupported=true;
                if(attribute.type==0x10) {
                    if(standard||attribute.nonResident||attribute.nameLength||le(bytes,12,2))invalid("invalid STANDARD_INFORMATION");
                    const auto value=bytes.subspan(le(bytes,20,2),le(bytes,16,4));
                    if(value.size()<36)invalid("short STANDARD_INFORMATION");
                    standard=true;
                    if(le(value,32,4)&0x10)invalid("file record has directory attributes");
                    if(le(value,32,4)&0x4e00)unsupported=true;
                }
                if(attribute.type==0x30) {
                    filename=true;
                    if(attribute.nonResident||attribute.nameLength||le(bytes,12,2))invalid("invalid FILE_NAME form");
                    const auto value=bytes.subspan(le(bytes,20,2),le(bytes,16,4));
                    if(value.size()<66)invalid("short FILE_NAME");
                    const auto parent=le(value,0,8),length=le(value,64,1),space=le(value,65,1);
                    if(!length||value.size()!=66+length*2||space>3)invalid("invalid FILE_NAME length or namespace");
                    if((parent&0x0000ffffffffffff)!=kRootRecord)continue;
                    rootLink=true;
                    if((parent>>48)!=rootSequence){staleRoot=true;continue;}
                    std::vector<uint16_t> units;units.reserve(length);
                    for(size_t j=0;j<length;++j)units.push_back(static_cast<uint16_t>(le(value,66+j*2,2)));
                    auto name=decodeSafeFileName(units);if(name.empty())continue;
                    names.insert(name);const int rank=space==2?0:(space==0?1:2);
                    if(rank>preferred){preferred=rank;selected=std::move(name);}
                }
                if(attribute.type==0x80&&!attribute.nameLength) {
                    if(hasData)invalid("multiple unnamed DATA attributes");
                    hasData=true;nonresident=attribute.nonResident;
                    if(le(bytes,12,2))unsupported=true;
                    if(nonresident)external=bytes;
                    if(!nonresident) {
                        const auto value=bytes.subspan(le(bytes,20,2),le(bytes,16,4));data.assign(value.begin(),value.end());
                    }
                }
            }
            if(selected.empty()) {
                if(staleRoot)invalid("obsolete root parent sequence");
                if(!filename)invalid("missing FILE_NAME");
                if(rootLink)invalid("no safe root filename");
                ++result.nonRootFiles;continue;
            }
            if(unsupported){++result.unsupportedFiles;continue;}
            if(!standard||!hasData)invalid("missing file metadata");
            if(nonresident) {
                if(!includeNonresident){++result.nonresidentFiles;continue;}
                const auto stream=parseData(external,clusters,clusterBytes,cancelled);
                if(stream.runs.size()>65536-claimed.size())invalid("file extent tracking limit reached");
                // Reject metadata/self/cross-file overlaps before reading any payload.
                for(const auto& run:stream.runs) {
                    const auto next=claimed.lower_bound(run.lcn);
                    if((next!=claimed.end()&&run.lcn+run.clusters>next->first)||
                       (next!=claimed.begin()&&std::prev(next)->second>run.lcn))invalid("file DATA overlaps MFT or an earlier file");
                }
                for(const auto& run:stream.runs)claimed.emplace(run.lcn,run.lcn+run.clusters);
                // Never expose bytes beyond ValidDataLength; the logical tail is zero-filled.
                data.assign(static_cast<size_t>(stream.logical),0);uint64_t copied=0;
                for(const auto& run:stream.runs) {
                    check();if(copied==stream.initialized)break;
                    const auto length=std::min(stream.initialized-copied,run.clusters*clusterBytes);
                    std::vector<uint8_t> chunk;
                    if(!readSourceBytes(&reader,start,count,run.lcn*clusterBytes,length,chunk,cancelled))invalid("file DATA source read failed");
                    std::copy(chunk.begin(),chunk.end(),data.begin()+copied);copied+=length;
                    result.processedBytes+=length;if(progress)progress(result.processedBytes);
                }
                if(copied!=stream.initialized)invalid("incomplete file DATA");
                recoveredNonresident=true;
            }
            if(names.size()>1)result.otherRootNames+=names.size()-1;
        }catch(const std::runtime_error&){check();++result.invalidFiles;continue;}
        check();item(selected,data);++result.foundItems;if(recoveredNonresident)++result.recoveredNonresidentFiles;if(progress)progress(result.processedBytes);
    }
    check();return result;
}

NtfsResidentSummary recoverNtfsResidentRootFiles(disk::IDiskReader& reader,uint64_t start,uint64_t count,
    const std::function<void(const std::string&,const std::vector<uint8_t>&)>& item,
    const std::function<void(uint64_t)>& progress,const std::function<bool()>& cancelled) {
    return recoverRootFiles(reader,start,count,item,progress,cancelled,false);
}
NtfsResidentSummary recoverNtfsRootFiles(disk::IDiskReader& reader,uint64_t start,uint64_t count,
    const std::function<void(const std::string&,const std::vector<uint8_t>&)>& item,
    const std::function<void(uint64_t)>& progress,const std::function<bool()>& cancelled) {
    return recoverRootFiles(reader,start,count,item,progress,cancelled,true);
}

}
