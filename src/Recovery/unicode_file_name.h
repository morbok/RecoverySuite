#pragma once
#include <cstdint>
#include <span>
#include <string>
namespace recoverysuite::recovery {
// Decode a bounded UTF-16 filename to UTF-8. Empty means invalid/unsafe or above 255 bytes.
// No lossy replacement, path separators, control characters or dot traversal are accepted.
inline std::string decodeSafeFileName(std::span<const uint16_t> units) {
    if(units.empty()||units.size()>255)return {};
    std::string name;
    for(size_t i=0;i<units.size();++i) {
        uint32_t code=units[i];
        if(code>=0xd800&&code<=0xdbff) {
            if(++i>=units.size()||units[i]<0xdc00||units[i]>0xdfff)return {};
            code=0x10000+((code-0xd800)<<10)+(units[i]-0xdc00);
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
}
