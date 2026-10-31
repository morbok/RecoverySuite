#include "ntfs_mft.h"
#include "source_bytes.h"
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
Stream parseMftData(Bytes record,uint64_t clusters,uint64_t clusterBytes,uint64_t mftCluster,const std::function<bool()>& cancelled) {
    Stream stream;bool found=false,ended=false;const auto used=le(record,24,4);
    for(size_t pos=le(record,20,2);pos+4<=used;) {
        if(cancelled&&cancelled())invalid("cancelled");
        const auto type=le(record,pos,4);if(type==0xffffffff){ended=true;break;}
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
        if(type==0x20)invalid("MFT attribute-list extensions are unsupported");
        if(type==0x80&&!nameLength) {
            if(found||form!=1||le(attribute,12,2)||le(attribute,16,8)||le(attribute,34,2))invalid("unsupported MFT DATA flags, form or extension");
            found=true;stream.allocated=le(attribute,40,8);stream.logical=le(attribute,48,8);stream.initialized=le(attribute,56,8);
            if(!stream.initialized||stream.initialized>kMaxSnapshotBytes||stream.initialized>stream.logical||stream.logical>stream.allocated)invalid("invalid stream sizes or initialized MFT above 16 MiB");
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
                   (next!=physical.begin()&&std::prev(next)->second>current))invalid("overlapping MFT runs");
                physical.emplace(current,current+runLength);stream.runs.push_back({current,runLength});total+=runLength;
            }
            if(!terminated||stream.runs.empty()||le(attribute,24,8)!=total-1||stream.allocated!=total*clusterBytes||stream.runs.front().lcn!=mftCluster)
                invalid("incomplete or inconsistent MFT mapping pairs");
        }
        pos+=length;
    }
    if(!ended||!found)invalid("missing MFT DATA attribute or end marker");
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
}
