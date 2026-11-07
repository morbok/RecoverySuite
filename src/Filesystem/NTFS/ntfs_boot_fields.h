#pragma once
#include "FilesystemReader.h"
#include <cstring>
#include <limits>
#include <optional>
#include <vector>
namespace recoverysuite::filesystem::ntfs {
struct NtfsBootFields {
    uint64_t sectors{},clusterBytes{},mftCluster{},mirrorCluster{},serial{},recordBytes{};
    uint32_t sectorBytes{};
};
// Byte-wise inspection; no packed-struct casts or assumed native byte order.
inline std::optional<NtfsBootFields> parseNtfsBootFields(const uint8_t* bytes,size_t size) {
    if(!bytes||size<512||std::memcmp(bytes+3,"NTFS    ",8)||bytes[510]!=0x55||bytes[511]!=0xaa)return {};
    const auto le=[&](size_t pos,size_t width){uint64_t value=0;for(size_t i=0;i<width;++i)value|=uint64_t{bytes[pos+i]}<<(8*i);return value;};
    const auto sector=le(11,2),spc=le(13,1),total=le(40,8),mft=le(48,8),mirror=le(56,8);
    if(sector<512||sector>4096||(sector&(sector-1))||!spc||spc>128||(spc&(spc-1))||!total||
       total>std::numeric_limits<uint64_t>::max()/sector||!mft||mft>=total/spc||mirror>=total/spc)return {};
    const auto code=le(64,1);uint64_t record=0;
    if(code>=128){const auto exponent=256-code;if(exponent<9||exponent>16)return {};record=uint64_t{1}<<exponent;}
    else record=code*sector*spc;
    if(record<512||record>65536||(record&(record-1)))return {};
    return NtfsBootFields{total,sector*spc,mft,mirror,le(72,8),record,static_cast<uint32_t>(sector)};
}
inline std::optional<NtfsBootFields> readNtfsBootFields(FilesystemReader* reader) {
    if(!reader)return {};
    const auto sector=reader->getSectorSize();
    if(sector<512||sector>4096||(sector&(sector-1)))return {};
    std::vector<uint8_t> bytes(sector);
    if(reader->readSector(0,bytes.data(),sector)!=sector)return {};
    auto fields=parseNtfsBootFields(bytes.data(),bytes.size());
    if(!fields||fields->sectorBytes!=sector||fields->sectors>reader->getTotalSectors())return {};
    return fields;
}
}
