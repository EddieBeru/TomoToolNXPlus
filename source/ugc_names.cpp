// Read-only UGC names: format/hashes from alexislours/ltd-save-editor,
// as attributed in TomoToolNX mii_manager.cpp. No save mutation.
#include "ugc_names.h"
#include <array>
#include <cstdio>
#include <set>
namespace UgcNames {
namespace {
uint32_t u32(const std::vector<uint8_t>& b,size_t at) {
    return uint32_t(b[at]) | uint32_t(b[at+1])<<8 | uint32_t(b[at+2])<<16 | uint32_t(b[at+3])<<24;
}
bool text(const uint8_t* b,std::string& out) {
    for(size_t i=0;i<64;++i) {
        uint32_t cp=b[i*2] | uint32_t(b[i*2+1])<<8;
        if(!cp)return true;
        if(cp>=0xd800 && cp<=0xdbff) {
            if(++i>=64)return false;
            uint32_t low=b[i*2] | uint32_t(b[i*2+1])<<8;
            if(low<0xdc00 || low>0xdfff)return false;
            cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);
        } else if(cp>=0xdc00 && cp<=0xdfff)return false;
        if(cp<0x80)out+=char(cp);
        else if(cp<0x800){out+=char(0xc0|(cp>>6));out+=char(0x80|(cp&63));}
        else if(cp<0x10000){out+=char(0xe0|(cp>>12));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}
        else{out+=char(0xf0|(cp>>18));out+=char(0x80|((cp>>12)&63));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}
    }
    return false;
}
}
Names parse(const std::vector<uint8_t>& b) {
    Names r;
    if(b.size()<0x20 || b.size()>8*1024*1024 || u32(b,0)!=0x01020304)return {{},true};
    size_t end=u32(b,8);
    if(end<0x20 || end>b.size() || (end-0x20)%8)return {{},true};
    struct Kind{const char* prefix;uint32_t hash;size_t capacity;};
    const std::array<Kind,7> kinds{{{"Food",0x408494f5,99},{"Cloth",0x40710642,299},
      {"Goods",0x2f793eb1,99},{"Interior",0x3de2c5dd,99},{"Exterior",0x27c875d6,99},
      {"MapObject",0x56f99338,99},{"MapFloor",0x918875a9,99}}};
    std::set<uint32_t> seen;uint32_t type=0;
    for(size_t pos=0x20;pos<end;pos+=8) {
        uint32_t hash=u32(b,pos),slot=u32(b,pos+4);
        if(!hash){if(slot>=33)return {{},true};type=slot;continue;}
        for(const auto& kind:kinds) if(hash==kind.hash) {
            if(type!=31 || !seen.insert(hash).second || slot<end || slot>b.size()-4)return {{},true};
            size_t count=u32(b,slot);
            if(count>kind.capacity || count>(b.size()-slot-4)/128)return {{},true};
            for(size_t i=0;i<count;++i) {
                std::string name;
                if(!text(b.data()+slot+4+i*128,name)){r.warning=true;continue;}
                if(name.empty())continue;
                char stem[40];std::snprintf(stem,sizeof(stem),"Ugc%s%03zu",kind.prefix,i);
                r.values[stem]=std::move(name);
            }
        }
    }
    return r;
}
}
