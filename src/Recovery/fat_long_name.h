#pragma once
#include "source_bytes.h"
#include <array>
#include <string>

namespace recoverysuite::recovery {
// LFN layout and checksum: Microsoft FAT Specification, section 7.
// https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf
// A bounded LFN sequence immediately preceding a short entry. Invalid sequences fall back
// to the short alias; no partial Unicode name or path component is exposed to the exporter.
class FatLongName {
public:
    void reset() { active_=false; seen_=false; expected_=0; length_=0; }
    void consume(const std::vector<uint8_t>& directory,size_t pos) {
        seen_=true;
        const auto ordinal=directory[pos];
        const auto number=ordinal&31;
        if((ordinal&0xa0)||!number||number>20||directory[pos+11]!=0x0f||
           directory[pos+12]!=0||sourceLe16(directory,pos+26)!=0) { active_=false;return; }
        if(ordinal&0x40) {
            active_=true;expected_=number;checksum_=directory[pos+13];length_=number*13;
        }
        if(!active_||number!=expected_||directory[pos+13]!=checksum_) { active_=false;return; }
        constexpr std::array<size_t,13> offsets{1,3,5,7,9,14,16,18,20,22,24,28,30};
        for(size_t i=0;i<offsets.size();++i)units_[(number-1)*13+i]=static_cast<uint16_t>(sourceLe16(directory,pos+offsets[i]));
        --expected_;
    }
    std::string take(const std::vector<uint8_t>& directory,size_t pos,bool& fallback) {
        auto name=resolve(directory,pos);fallback=seen_&&name.empty();reset();return name;
    }
private:
    std::string resolve(const std::vector<uint8_t>& directory,size_t pos) const {
        if(!active_||expected_)return {};
        uint8_t checksum=0;
        for(size_t i=0;i<11;++i)checksum=static_cast<uint8_t>((checksum>>1)|((checksum&1)<<7))+directory[pos+i];
        if(checksum!=checksum_)return {};
        size_t count=0;
        while(count<length_&&units_[count]!=0)++count;
        if(!count||count>255||(count+12)/13!=length_/13)return {};
        if(count<length_)for(size_t i=count+1;i<length_;++i)if(units_[i]!=0xffff)return {};
        std::string name;
        for(size_t i=0;i<count;++i) {
            uint32_t code=units_[i];
            if(code>=0xd800&&code<=0xdbff) {
                if(++i>=count||units_[i]<0xdc00||units_[i]>0xdfff)return {};
                code=0x10000+((code-0xd800)<<10)+(units_[i]-0xdc00);
            }else if(code>=0xdc00&&code<=0xdfff)return {};
            if(code==0xffff||code<32||(code>=127&&code<=159)||
               (code<128&&std::string("\"*/:<>?\\|").find(static_cast<char>(code))!=std::string::npos))return {};
            if(code<0x80)name.push_back(static_cast<char>(code));
            else if(code<0x800) {
                name.push_back(static_cast<char>(0xc0|(code>>6)));name.push_back(static_cast<char>(0x80|(code&63)));
            }else if(code<0x10000) {
                name.push_back(static_cast<char>(0xe0|(code>>12)));name.push_back(static_cast<char>(0x80|((code>>6)&63)));name.push_back(static_cast<char>(0x80|(code&63)));
            }else {
                name.push_back(static_cast<char>(0xf0|(code>>18)));name.push_back(static_cast<char>(0x80|((code>>12)&63)));
                name.push_back(static_cast<char>(0x80|((code>>6)&63)));name.push_back(static_cast<char>(0x80|(code&63)));
            }
            if(name.size()>255)return {};
        }
        if(name=="."||name==".."||name.back()=='.'||name.back()==' ')return {};
        return name;
    }
    std::array<uint16_t,260> units_{};
    bool active_{},seen_{};
    uint8_t expected_{},checksum_{};
    size_t length_{};
};
}
