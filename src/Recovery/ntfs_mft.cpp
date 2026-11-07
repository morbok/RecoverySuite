#include "ntfs_mft.h"
#include "source_bytes.h"
#include "unicode_file_name.h"
#include <set>
#include <deque>
#include <tuple>
#include <optional>
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
    if((!mft&&(stream.initialized>kMaxSnapshotBytes||stream.logical>kMaxSnapshotBytes))||stream.initialized>stream.logical||stream.logical>stream.allocated)invalid("invalid DATA sizes or stream above 16 MiB");
    size_t cursor=le(attribute,32,2);
    if(cursor<header||cursor>=length||(le(attribute,9,1)&&le(attribute,10,2)+le(attribute,9,1)*2>cursor))invalid("invalid mapping-pairs offset");
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
constexpr uint64_t kMaxFileBytes=uint64_t{2}*1024*1024*1024;
struct CheckedData {Stream stream;std::vector<uint8_t> resident;bool external=false;uint64_t size()const{return external?stream.logical:resident.size();}};
class NtfsContext {
public:
    disk::IDiskReader& reader;uint64_t start,count,clusterBytes,clusters,processed{};
    NtfsMftSnapshot mft;std::function<void(uint64_t)> progress;std::function<bool()> cancelled;
    std::map<uint64_t,std::vector<uint8_t>> decoded;
    std::map<uint64_t,std::vector<Attribute>> attributesCache;
    std::map<uint64_t,uint64_t> metadataClaims;
    std::vector<uint8_t> volumeBitmap,recordBitmap;
    std::vector<std::pair<Stream,bool>> listStreams;
    std::map<uint64_t,std::set<std::pair<uint64_t,std::string>>> indexes;
    uint64_t clusterChecks=0,indexEntries=0;
    NtfsContext(disk::IDiskReader& r,uint64_t s,uint64_t n,const std::function<void(uint64_t)>& p,const std::function<bool()>& c)
        :reader(r),start(s),count(n),mft(readNtfsMft(r,s,n,p,c,128*1024*1024)),progress(p),cancelled(c) {
        clusterBytes=le(mft.bootSector,11,2)*le(mft.bootSector,13,1);clusters=le(mft.bootSector,40,8)/le(mft.bootSector,13,1);processed=mft.processedBytes;
        const auto reserved=parseMftData(record(0),clusters,clusterBytes,le(mft.bootSector,48,8),cancelled);
        claim(reserved);
    }
    void check()const{if(cancelled&&cancelled())invalid("cancelled");}
    uint64_t slots()const{return mft.initializedRecords.size()/mft.recordSize;}
    Bytes record(uint64_t index) {
        check();if(index>=slots())invalid("record reference outside initialized MFT");
        if(auto it=decoded.find(index);it!=decoded.end())return it->second;
        if(decoded.size()*mft.recordSize>=128*1024*1024)invalid("decoded-record memory limit");
        const auto raw=Bytes(mft.initializedRecords).subspan(index*mft.recordSize,mft.recordSize);
        return decoded.emplace(index,decodeRecord(raw)).first->second;
    }
    std::vector<uint8_t> source(uint64_t offset,uint64_t length) {
        check();std::vector<uint8_t> result;
        if(!length)return result;
        if(offset>clusters*clusterBytes||length>clusters*clusterBytes-offset||!readSourceBytes(&reader,start,count,offset,length,result,cancelled))invalid("checked source read failed");
        processed+=length;if(progress)progress(processed);return result;
    }
    static std::string attributeName(const Attribute& a) {
        std::string result;for(size_t i=0;i<a.nameLength*2;++i)result.push_back(char(a.bytes[le(a.bytes,10,2)+i]));return result;
    }
    CheckedData data(const std::vector<Attribute>& attrs,uint64_t type,const std::string& name={}) {
        std::vector<Attribute> pieces;
        for(const auto& a:attrs)if(a.type==type&&attributeName(a)==name)pieces.push_back(a);
        if(pieces.empty())invalid("required attribute is missing");
        CheckedData result;
        if(!pieces.front().nonResident) {
            if(pieces.size()!=1||le(pieces.front().bytes,12,2))invalid("duplicate/flagged resident attribute");
            const auto a=pieces.front().bytes;const auto value=a.subspan(le(a,20,2),le(a,16,4));result.resident.assign(value.begin(),value.end());return result;
        }
        result.external=true;
        std::sort(pieces.begin(),pieces.end(),[](const auto& a,const auto& b){return le(a.bytes,16,8)<le(b.bytes,16,8);});
        uint64_t next=0;std::map<uint64_t,uint64_t> physical;
        for(const auto& piece:pieces) {
            check();if(!piece.nonResident||le(piece.bytes,16,8)!=next)invalid("missing/overlapping DATA extension VCNs");
            const auto low=le(piece.bytes,16,8),high=le(piece.bytes,24,8);
            if(high<low||high-low>=clusters)invalid("invalid extension VCN range");
            std::vector<uint8_t> copy(piece.bytes.begin(),piece.bytes.end());
            const auto put=[&](size_t offset,uint64_t value){for(int i=0;i<8;++i)copy[offset+i]=uint8_t(value>>(8*i));};
            put(16,0);put(24,high-low);put(40,(high-low+1)*clusterBytes);put(48,(high-low+1)*clusterBytes);put(56,(high-low+1)*clusterBytes);
            const auto part=parseData(copy,clusters,clusterBytes,cancelled,true);
            if(result.stream.runs.size()+part.runs.size()>kMaxRuns)invalid("extended stream run limit");
            for(const auto& run:part.runs) {
                const auto it=physical.lower_bound(run.lcn);
                if((it!=physical.end()&&it->first<run.lcn+run.clusters)||(it!=physical.begin()&&std::prev(it)->second>run.lcn))invalid("overlapping extension physical runs");
                physical.emplace(run.lcn,run.lcn+run.clusters);result.stream.runs.push_back(run);
            }
            next=high+1;
        }
        const auto first=pieces.front().bytes;
        result.stream.allocated=le(first,40,8);result.stream.logical=le(first,48,8);result.stream.initialized=le(first,56,8);
        if(result.stream.allocated!=next*clusterBytes||result.stream.logical>result.stream.allocated||result.stream.initialized>result.stream.logical||result.stream.logical>kMaxFileBytes)invalid("invalid extended stream sizes");
        return result;
    }
    std::vector<uint8_t> read(const CheckedData& data,uint64_t offset,size_t length) {
        check();if(length>65536||offset>data.size()||length>data.size()-offset)invalid("invalid streamed read request");
        if(!data.external)return {data.resident.begin()+offset,data.resident.begin()+offset+length};
        std::vector<uint8_t> result(length,0);const auto initialized=data.stream.initialized;
        if(offset>=initialized)return result;
        const auto end=std::min<uint64_t>(offset+length,initialized);uint64_t vcnOffset=0,filled=0;
        for(const auto& run:data.stream.runs) {
            const auto bytes=run.clusters*clusterBytes;
            if(offset<vcnOffset+bytes&&end>vcnOffset) {
                const auto begin=std::max(offset,vcnOffset),finish=std::min(end,vcnOffset+bytes);
                const auto chunk=source(run.lcn*clusterBytes+begin-vcnOffset,finish-begin);
                std::copy(chunk.begin(),chunk.end(),result.begin()+begin-offset);filled+=chunk.size();
            }
            vcnOffset+=bytes;if(vcnOffset>=end)break;
        }
        if(filled!=end-offset)invalid("incomplete streamed data");
        return result;
    }
    std::vector<uint8_t> materialize(const CheckedData& d,uint64_t limit=16*1024*1024) {
        if(d.size()>limit||(d.external&&d.stream.initialized!=d.size()))invalid("metadata stream too large or not initialized");
        std::vector<uint8_t> result;result.reserve(d.size());
        for(uint64_t offset=0;offset<d.size();) {const auto chunk=read(d,offset,std::min<uint64_t>(65536,d.size()-offset));result.insert(result.end(),chunk.begin(),chunk.end());offset+=chunk.size();}
        return result;
    }
    const std::vector<Attribute>& attributes(uint64_t index) {
        if(auto it=attributesCache.find(index);it!=attributesCache.end())return it->second;
        const auto base=record(index);auto attrs=listAttributes(base,cancelled);const auto baseReference=index|(le(base,16,2)<<48);
        std::vector<Attribute> lists;for(const auto& a:attrs)if(a.type==0x20)lists.push_back(a);
        if(lists.size()>1)invalid("multiple ATTRIBUTE_LIST streams");
        if(!lists.empty()) {
            if(lists[0].nameLength)invalid("named ATTRIBUTE_LIST");
            const auto listData=data(lists,0x20);
            if(listData.external){claim(listData.stream);const bool used=le(base,22,2)&1;listStreams.emplace_back(listData.stream,used);if(!volumeBitmap.empty())allocated(listData.stream,used);}
            const auto content=materialize(listData,1024*1024);std::set<std::tuple<uint64_t,uint64_t,uint64_t>> seen;std::set<uint64_t> extensions;
            size_t entries=0;
            for(size_t pos=0;pos<content.size();) {
                check();if(content.size()-pos<26||++entries>1024)invalid("invalid/excessive ATTRIBUTE_LIST entries");
                const auto bytes=Bytes(content).subspan(pos);const auto type=le(bytes,0,4),size=le(bytes,4,2),names=le(bytes,6,1),nameOffset=le(bytes,7,1),low=le(bytes,8,8),ref=le(bytes,16,8),instance=le(bytes,24,2);
                if(size<26||size%8||size>bytes.size()||(names&&(nameOffset<26||nameOffset+names*2>size))||!seen.emplace(ref,instance,type).second)invalid("malformed/duplicate ATTRIBUTE_LIST entry");
                const auto target=ref&0x0000ffffffffffff;const auto extension=record(target);
                if(le(extension,16,2)!=(ref>>48)||!le(extension,16,2)||(le(extension,22,2)&1)!=(le(base,22,2)&1))invalid("stale ATTRIBUTE_LIST reference");
                if(target!=index) {
                    if(le(extension,32,8)!=baseReference)invalid("invalid extension base reference");
                    extensions.insert(target);
                    if(extensions.size()>64)invalid("extension record limit");
                }
                std::string name;if(names)name.assign(reinterpret_cast<const char*>(bytes.data()+nameOffset),names*2);
                size_t matches=0;
                for(const auto& a:listAttributes(extension,cancelled)) {
                    if(a.type==0x20&&target!=index)invalid("nested ATTRIBUTE_LIST unsupported");
                    if(a.type==type&&le(a.bytes,14,2)==instance&&attributeName(a)==name&&(a.nonResident?le(a.bytes,16,8):0)==low) {
                        ++matches;if(target!=index)attrs.push_back(a);
                    }
                }
                if(matches!=1)invalid("ATTRIBUTE_LIST entry does not match its attribute");
                pos+=size;
            }
        }
        return attributesCache.emplace(index,std::move(attrs)).first->second;
    }
    void claim(const Stream& stream) {
        if(metadataClaims.size()+stream.runs.size()>65536)invalid("metadata extent limit");
        for(const auto& run:stream.runs) {
            uint64_t begin=run.lcn,end=run.lcn+run.clusters;auto it=metadataClaims.lower_bound(begin);
            if(it!=metadataClaims.begin()&&std::prev(it)->second>=begin){--it;begin=it->first;}
            while(it!=metadataClaims.end()&&it->first<=end){end=std::max(end,it->second);it=metadataClaims.erase(it);}
            metadataClaims.emplace(begin,end);
        }
    }
    static bool bit(const std::vector<uint8_t>& bytes,uint64_t index) {if(index/8>=bytes.size())invalid("allocation bitmap is truncated");return bytes[index/8]&(1u<<(index%8));}
    void allocation(bool protectLive=false) {
        const auto mftBitmap=data(attributes(0),0xb0);recordBitmap=materialize(mftBitmap);if(mftBitmap.external)claim(mftBitmap.stream);
        const auto bitmapRecord=record(6);if(le(bitmapRecord,22,2)!=1||le(bitmapRecord,32,8))invalid("invalid volume bitmap base record");
        const auto bitmapData=data(attributes(6),0x80);volumeBitmap=materialize(bitmapData);if(bitmapData.external)claim(bitmapData.stream);
        if(recordBitmap.size()<(slots()+7)/8||volumeBitmap.size()<(clusters+7)/8)invalid("short allocation bitmap");
        for(uint64_t i=0;i<slots();++i) {
            check();const auto raw=Bytes(mft.initializedRecords).subspan(i*mft.recordSize,mft.recordSize);
            const bool blank=std::all_of(raw.begin(),raw.end(),[](uint8_t c){return c==0;});
            if(bit(recordBitmap,i)!=(!blank&&(le(raw,22,2)&1)))invalid("MFT bitmap and record flags disagree");
        }
        allocated(parseMftData(record(0),clusters,clusterBytes,le(mft.bootSector,48,8),cancelled),true);
        if(mftBitmap.external)allocated(mftBitmap.stream,true);
        if(bitmapData.external)allocated(bitmapData.stream,true);
        for(const auto& [stream,used]:listStreams)allocated(stream,used);
        for(uint64_t i=0;i<slots();++i) {
            const auto raw=Bytes(mft.initializedRecords).subspan(i*mft.recordSize,mft.recordSize);
            if(!(le(raw,22,2)&1)||le(raw,32,8))continue;
            std::set<std::pair<uint64_t,std::string>> streams;
            for(const auto& a:attributes(i))if(a.nonResident&&(i<16||a.type!=0x80||a.nameLength||protectLive))streams.emplace(a.type,attributeName(a));
            for(const auto& [type,name]:streams){const auto d=data(attributes(i),type,name);allocated(d.stream,true);claim(d.stream);}
        }
    }
    void allocated(const Stream& stream,bool used) {
        for(const auto& run:stream.runs)for(uint64_t c=run.lcn;c<run.lcn+run.clusters;++c){if(++clusterChecks>64*1024*1024)invalid("allocation-check work limit");if((c&4095)==0)check();if(bit(volumeBitmap,c)!=used)invalid(used?"file references a free cluster":"deleted file cluster has been reallocated");}
    }
    std::string fileName(Bytes value) {
        if(value.size()<66)invalid("short FILE_NAME key");
        const auto length=le(value,64,1),space=le(value,65,1);
        if(!length||value.size()!=66+length*2||space>3)invalid("invalid FILE_NAME key");
        std::vector<uint16_t> units;for(size_t j=0;j<length;++j)units.push_back(le(value,66+2*j,2));
        const auto name=decodeSafeFileName(units);if(name.empty())invalid("unsafe FILE_NAME key");return name;
    }
    const std::set<std::pair<uint64_t,std::string>>& index(uint64_t directory) {
        if(auto it=indexes.find(directory);it!=indexes.end())return it->second;
        const std::string i30{"$\0I\0" "3\0" "0\0",8};const auto& attrs=attributes(directory);
        for(const auto& a:attrs)if(a.type==0x90&&attributeName(a)==i30&&a.nonResident)invalid("nonresident INDEX_ROOT");
        const auto root=materialize(data(attrs,0x90,i30));
        if(root.size()<32||le(root,0,4)!=0x30||le(root,4,4)!=1)invalid("invalid directory INDEX_ROOT");
        const auto blockSize=le(root,8,4);if(blockSize<512||blockSize>65536||(blockSize&(blockSize-1)))invalid("invalid index block size");
        std::optional<CheckedData> allocationData;std::vector<uint8_t> bitmap;std::set<uint64_t> visited;
        std::set<std::pair<uint64_t,std::string>> entries;
        const auto parent=directory|(le(record(directory),16,2)<<48);
        const auto walk=[&](auto&& self,Bytes bytes,size_t header,size_t depth)->void {
            check();if(depth>32||bytes.size()<header+16)invalid("invalid index depth/header");
            const auto first=le(bytes,header,4),used=le(bytes,header+4,4),allocated=le(bytes,header+8,4),flags=le(bytes,header+12,1);
            if(first<16||first%8||used<first+16||used>allocated||allocated>bytes.size()-header||flags>1)invalid("invalid INDEX_HEADER bounds");
            bool last=false;
            for(size_t pos=header+first;pos<header+used;) {
                if(header+used-pos<16)invalid("short INDEX_ENTRY");
                const auto length=le(bytes,pos+8,2),key=le(bytes,pos+10,2),f=le(bytes,pos+12,2);
                if(length<uint64_t{16}+((f&1)?8:0)||length%8||length>header+used-pos||f>3||key>length-16-((f&1)?8:0))invalid("invalid INDEX_ENTRY bounds");
                if(!(f&2)) {
                    const auto value=bytes.subspan(pos+16,key);if(le(value,0,8)!=parent)invalid("index key has wrong parent");
                    const auto ref=le(bytes,pos,8);if((ref&0x0000ffffffffffff)>=slots()||!(ref>>48))invalid("index record reference outside MFT");
                    if(!entries.emplace(ref,fileName(value)).second)invalid("duplicate directory index entry");
                    if(++indexEntries>100000)invalid("index entry limit");
                }else {if(key)invalid("end index entry has a key");last=true;}
                if(f&1) {
                    if(!flags)invalid("leaf index has child pointer");
                    if(!allocationData){allocationData=data(attrs,0xa0,i30);if(!allocationData->external)invalid("resident INDEX_ALLOCATION");bitmap=materialize(data(attrs,0xb0,i30));}
                    const auto child=le(bytes,pos+length-8,8),unit=blockSize>=clusterBytes?clusterBytes:512;
                    if(child>std::numeric_limits<uint64_t>::max()/unit)invalid("index VCN overflow");
                    const auto offset=child*unit;
                    if(offset%blockSize||offset>allocationData->size()||blockSize>allocationData->size()-offset||!bit(bitmap,offset/blockSize)||!visited.insert(child).second||visited.size()>4096)invalid("invalid/cyclic index child");
                    auto block=read(*allocationData,offset,blockSize);if(!std::equal(block.begin(),block.begin()+4,"INDX")||le(block,16,8)!=child)invalid("invalid INDX signature/VCN");
                    const auto usa=le(block,4,2),words=le(block,6,2);
                    if(usa<40||usa%2||words!=blockSize/512+1||usa+words*2>512-2||usa+words*2>24+le(block,24,4))invalid("invalid INDX update sequence array");
                    const auto tag=le(block,usa,2);for(size_t i=1;i<words;++i){if(le(block,i*512-2,2)!=tag)invalid("torn INDX block");block[i*512-2]=block[usa+i*2];block[i*512-1]=block[usa+i*2+1];}
                    self(self,block,24,depth+1);
                }
                pos+=length;if(last){if(pos!=header+used)invalid("entries follow index end");break;}
            }
            if(!last)invalid("missing index end entry");
        };
        walk(walk,root,16,0);
        if(allocationData) {
            const auto blocks=allocationData->size()/blockSize;
            if(allocationData->size()%blockSize||bitmap.size()<(blocks+7)/8)invalid("truncated index allocation/bitmap");
            for(uint64_t i=0;i<bitmap.size()*8;++i)if(bit(bitmap,i)) {
                if(i>=blocks||!visited.contains(i*blockSize/(blockSize>=clusterBytes?clusterBytes:512)))invalid("unreachable allocated index block");
            }
        }
        return indexes.emplace(directory,std::move(entries)).first->second;
    }
};

}
NtfsMftSnapshot readNtfsMft(disk::IDiskReader& reader,uint64_t start,uint64_t count,
    const std::function<void(uint64_t)>& progress,const std::function<bool()>& cancelled,uint64_t maximumBytes) {
    if(!maximumBytes||maximumBytes>128*1024*1024)invalid("MFT resource limit outside 1..128 MiB");
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
    if(stream.initialized>maximumBytes||stream.runs.front().clusters*clusterBytes<result.recordSize||stream.initialized%result.recordSize||stream.logical%result.recordSize||stream.initialized<result.recordSize)
        invalid("fragmented bootstrap record or incomplete FILE record lengths");
    result.initializedRecords.reserve(static_cast<size_t>(stream.initialized));
    for(const auto& run:stream.runs) {
        check();const auto remaining=stream.initialized-result.initializedRecords.size();if(!remaining)break;
        const auto length=std::min(remaining,run.clusters*clusterBytes);
        for(uint64_t offset=0;offset<length;) {
            const auto chunk=std::min<uint64_t>(length-offset,65536);const auto bytes=read(run.lcn*clusterBytes+offset,chunk);
            result.initializedRecords.insert(result.initializedRecords.end(),bytes.begin(),bytes.end());offset+=chunk;
        }
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
    const std::function<void(uint64_t)>& progress,const std::function<bool()>& cancelled,bool includeNonresident,bool recursive=false) {
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
    std::map<uint64_t,std::string> directoryPaths{{5,{}}};
    const auto directoryPath=[&](auto&& self,uint64_t reference,std::set<uint64_t>& visiting)->std::string {
        check();const auto index=reference&0x0000ffffffffffff,sequence=reference>>48;
        if(index>=slots||!sequence)invalid("directory reference outside initialized MFT");
        const auto directory=decodeRecord(records.subspan(index*mft.recordSize,mft.recordSize));
        if(le(directory,22,2)!=3||le(directory,32,8)||le(directory,16,2)!=sequence)invalid("stale or invalid parent directory");
        if(auto it=directoryPaths.find(index);it!=directoryPaths.end())return it->second;
        if(visiting.size()>=32||!visiting.insert(index).second||directoryPaths.size()>=1024)invalid("directory cycle, depth or count limit");
        int rank=-1;std::string selected;
        for(const auto& attribute:listAttributes(directory,cancelled)) {
            if(attribute.type==0x20||attribute.type==0xc0)invalid("unsupported directory extension or reparse point");
            if(attribute.type!=0x30)continue;
            if(attribute.nonResident||attribute.nameLength||le(attribute.bytes,12,2))invalid("invalid directory FILE_NAME");
            const auto value=attribute.bytes.subspan(le(attribute.bytes,20,2),le(attribute.bytes,16,4));
            if(value.size()<66)invalid("short directory name");
            const auto length=le(value,64,1),space=le(value,65,1);
            if(!length||value.size()!=66+length*2||space>3)invalid("invalid directory name bounds");
            std::vector<uint16_t> units;for(size_t j=0;j<length;++j)units.push_back(le(value,66+2*j,2));
            auto name=decodeSafeFileName(units);if(name.empty())continue;
            const int candidate=space==2?0:(space==0?1:2);if(candidate<=rank)continue;
            const auto parent=self(self,le(value,0,8),visiting);
            selected=parent.empty()?name:parent+'/'+name;rank=candidate;
        }
        visiting.erase(index);
        if(selected.empty()||selected.size()>4095||std::count(selected.begin(),selected.end(),'/')>=32)invalid("missing or excessive directory path");
        directoryPaths.emplace(index,selected);return selected;
    };
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
                    if(!recursive&&(parent&0x0000ffffffffffff)!=kRootRecord)continue;
                    rootLink=true;
                    if((parent&0x0000ffffffffffff)==kRootRecord&&(parent>>48)!=rootSequence){staleRoot=true;continue;}
                    std::vector<uint16_t> units;units.reserve(length);
                    for(size_t j=0;j<length;++j)units.push_back(static_cast<uint16_t>(le(value,66+j*2,2)));
                    auto name=decodeSafeFileName(units);if(name.empty())continue;
                    if(recursive){std::set<uint64_t> visiting;const auto path=directoryPath(directoryPath,parent,visiting);if(!path.empty())name=path+'/'+name;}
                    if(name.size()>4095)invalid("file path exceeds 4095 bytes");
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

NtfsResidentSummary recoverNtfsTreeFiles(disk::IDiskReader& reader,uint64_t start,uint64_t count,
    const std::function<void(const std::string&,const std::vector<uint8_t>&)>& item,
    const std::function<void(uint64_t)>& progress,const std::function<bool()>& cancelled) {
    return recoverRootFiles(reader,start,count,item,progress,cancelled,true,true);
}

NtfsRecoverySummary recoverNtfsChecked(disk::IDiskReader& reader,uint64_t start,uint64_t count,
    const NtfsRecoveryOptions& options,const std::function<void(const NtfsFileSource&)>& item,
    const std::function<void(uint64_t)>& progress,const std::function<bool()>& cancelled) {
    NtfsContext context(reader,start,count,progress,cancelled);NtfsRecoverySummary result;
    if(options.deleted&&!options.verifyAllocation)invalid("deleted recovery requires allocation verification");
    if(options.verifyAllocation)context.allocation(options.deleted);
    const auto root=context.record(5);if(le(root,22,2)!=3||le(root,32,8)||!le(root,16,2))invalid("invalid root directory");
    std::map<uint64_t,std::string> paths{{5,{}}};
    const auto path=[&](auto&& self,uint64_t reference,std::set<uint64_t>& visiting)->std::string {
        context.check();const auto number=reference&0x0000ffffffffffff;const auto record=context.record(number);
        if(le(record,16,2)!=(reference>>48)||!le(record,16,2)||(le(record,22,2)!=3&&!(options.deleted&&le(record,22,2)==2))||le(record,32,8))invalid("stale or invalid parent directory");
        if(auto it=paths.find(number);it!=paths.end())return it->second;
        if(!visiting.insert(number).second||visiting.size()>32||paths.size()>=1024)invalid("directory cycle/count/depth limit");
        std::string selected;int rank=-1;bool standard=false;
        for(const auto& a:context.attributes(number)) {
            if(a.type==0xc0)invalid("reparse parent directory");
            if(a.type==0x10){if(standard||a.nonResident||a.nameLength||le(a.bytes,12,2))invalid("invalid parent STANDARD_INFORMATION");standard=true;const auto value=a.bytes.subspan(le(a.bytes,20,2),le(a.bytes,16,4));if(value.size()<36||(le(value,32,4)&0x4e00))invalid("unsupported parent directory");}
            if(a.type!=0x30)continue;
            if(a.nonResident||a.nameLength||le(a.bytes,12,2))invalid("invalid parent FILE_NAME");
            const auto value=a.bytes.subspan(le(a.bytes,20,2),le(a.bytes,16,4));const auto name=context.fileName(value);const auto parent=le(value,0,8);
            const int candidate=le(value,65,1)==2?0:(le(value,65,1)==0?1:2);if(candidate<=rank)continue;
            const auto prefix=self(self,parent,visiting);
            if(options.verifyAllocation&&!options.deleted&&!context.index(parent&0x0000ffffffffffff).contains({reference,name}))invalid("directory is missing from parent index");
            selected=prefix.empty()?name:prefix+'/'+name;rank=candidate;
        }
        visiting.erase(number);if(!standard||selected.empty()||selected.size()>4095||std::count(selected.begin(),selected.end(),'/')>=32)invalid("unsafe or excessive directory path");
        paths.emplace(number,selected);return selected;
    };
    for(uint64_t number=16;number<context.slots();++number) {
        context.check();const auto raw=Bytes(context.mft.initializedRecords).subspan(number*context.mft.recordSize,context.mft.recordSize);
        if(std::all_of(raw.begin(),raw.end(),[](uint8_t c){return !c;}))continue;
        const auto record=context.record(number);const auto flags=le(record,22,2);
        if(le(record,32,8)){++result.extensionRecords;continue;}
        if((flags&2)||bool(flags&1)==options.deleted){++result.skippedRecords;continue;}
        if(result.foundItems+result.invalidFiles>=10000)invalid("output item limit");
        if(options.deleted)++result.deletedCandidates;
        try {
            if(!le(record,16,2))invalid("zero record sequence");
            const auto& attrs=context.attributes(number);bool standard=false;std::string selected;int rank=-1;
            for(const auto& a:attrs) {
                if(a.type==0xc0)invalid("reparse file unsupported");
                if(a.type==0x10) {
                    if(standard||a.nonResident||a.nameLength||le(a.bytes,12,2))invalid("invalid STANDARD_INFORMATION");
                    const auto value=a.bytes.subspan(le(a.bytes,20,2),le(a.bytes,16,4));if(value.size()<36||(le(value,32,4)&0x10004e10))invalid("unsupported file attributes");standard=true;
                }
                if(a.type!=0x30)continue;
            if(a.nonResident||a.nameLength||le(a.bytes,12,2))invalid("invalid FILE_NAME");
                const auto value=a.bytes.subspan(le(a.bytes,20,2),le(a.bytes,16,4));const auto name=context.fileName(value);const auto parent=le(value,0,8);
                if(!options.recursive&&(parent&0x0000ffffffffffff)!=5)continue;
                std::set<uint64_t> visiting;const auto prefix=path(path,parent,visiting);
                const auto reference=number|(le(record,16,2)<<48);
                const auto candidate=le(value,65,1)==2?0:(le(value,65,1)==0?1:2);if(candidate<=rank)continue;
                if(options.verifyAllocation&&!options.deleted&&!context.index(parent&0x0000ffffffffffff).contains({reference,name}))invalid("file is missing from parent directory index");
                selected=prefix.empty()?name:prefix+'/'+name;rank=candidate;
            }
            if(!standard||selected.empty()||selected.size()>4095)invalid("missing metadata or unsafe file path");
            const auto data=context.data(attrs,0x80);
            if(data.external) {
                if(options.verifyAllocation)context.allocated(data.stream,!options.deleted);
                for(const auto& run:data.stream.runs) {
                    const auto it=context.metadataClaims.lower_bound(run.lcn);
                    if((it!=context.metadataClaims.end()&&it->first<run.lcn+run.clusters)||(it!=context.metadataClaims.begin()&&std::prev(it)->second>run.lcn))invalid("file stream overlaps metadata or previous file");
                }
                context.claim(data.stream);
            }
            NtfsFileSource file{selected,options.deleted?"Deleted metadata candidate; original content may have been overwritten":"Allocation and parent index checked; content validity is not established",data.size(),
                [&](uint64_t offset,size_t length){return context.read(data,offset,length);}};
            item(file);++result.foundItems;
        }catch(const std::runtime_error&){context.check();++result.invalidFiles;}
    }
    result.processedBytes=context.processed;return result;
}

}
