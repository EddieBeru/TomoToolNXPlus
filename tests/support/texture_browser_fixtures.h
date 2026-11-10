#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace Fixtures {
using Files = std::map<std::string, std::vector<uint8_t>>;
inline Files collection(size_t count, uint8_t color = 0) {
    Files files;
    for (size_t i=0; i<count; ++i) {
        auto number = std::to_string(i);
        files["/Ugc/UgcFood" + std::string(3-number.size(), '0') + number + ".ugctex.zs"]
            = std::vector<uint8_t>(131072, color);
    }
    return files;
}
inline void u32(std::vector<uint8_t>& bytes, size_t at, uint32_t value) {
    for (size_t i=0;i<4;++i) bytes.at(at+i)=uint8_t(value>>(i*8));
}
inline std::vector<uint8_t> names() {
    std::vector<uint8_t> bytes(0x40+4+128,0);
    u32(bytes,0,0x01020304); u32(bytes,8,0x30);
    u32(bytes,0x24,31); u32(bytes,0x28,0x408494f5); u32(bytes,0x2c,0x40);
    u32(bytes,0x40,1); bytes[0x44]='A'; return bytes;
}
}
#include "texture_browser.h"
namespace Fixtures {
class Source : public TextureBrowser::ReadSource {
public:
    Files files=collection(1);
    std::map<uint64_t,Files> accountFiles;
    const Files& activeFiles() const {auto it=accountFiles.find(account.words[0]);return it==accountFiles.end()?files:it->second;}
    std::vector<std::string> operations;
    TextureBrowser::Status failure;
    TextureBrowser::AccountKey account;
    TextureBrowser::Result<std::vector<TextureBrowser::Account>> accounts(const TextureBrowser::Cancel&) override {
        return {{{{{1,0}}, "Same",1,false}, {{{2,0}},"Same",2,false}}, {}};
    }
    TextureBrowser::Status open(TextureBrowser::AccountKey key,const TextureBrowser::Cancel&) override {
        operations.push_back("open-read"); account=key; return failure;
    }
    TextureBrowser::Result<std::vector<TextureBrowser::File>> enumerate(const TextureBrowser::Cancel&) override {
        TextureBrowser::Result<std::vector<TextureBrowser::File>> r;
        r.status=failure;
        for (auto& item:activeFiles()) if(item.first.rfind("/Ugc/",0)==0)
            r.value.push_back({item.first.substr(5),item.second.size()});
        return r;
    }
    TextureBrowser::Result<std::vector<uint8_t>> read(const std::string& ref,size_t limit,const TextureBrowser::Cancel& cancel) override {
        operations.push_back("read");
        if(cancel()) return {{},{TextureBrowser::Error::Cancelled}};
        auto it=activeFiles().find(ref);
        if(it==activeFiles().end()) return {{},{TextureBrowser::Error::ShortRead}};
        if(it->second.size()>limit) return {{},{TextureBrowser::Error::Limit}};
        return {it->second, failure};
    }
    void close() override {operations.push_back("close");}
};
}
