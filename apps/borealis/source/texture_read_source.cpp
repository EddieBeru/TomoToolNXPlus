#include "texture_read_source.hpp"
#include <switch.h>
#include <algorithm>
#include <cstring>
#include <set>

namespace TextureBrowser {
namespace {
template<class F> struct Finally { F action; ~Finally(){action();} };
template<class F> Finally<F> finally(F f){return {f};}
class SwitchSource final:public ReadSource {
    FsFileSystem fs{};
    bool opened=false;
    std::set<std::string> allowed;
public:
    ~SwitchSource() override {close();}
    Result<std::vector<Account>> accounts(const Cancel& cancel) override {
        Result<std::vector<Account>> result;
        if(cancel()) return {{},{Error::Cancelled}};
        ::Result rc=accountInitialize(AccountServiceType_Application);
        if(R_FAILED(rc))rc=accountInitialize(AccountServiceType_System);
        if(R_FAILED(rc))return {{},{Error::AccountService,rc}};
        auto cleanup=finally([]{accountExit();});
        AccountUid ids[ACC_USER_LIST_SIZE]{};s32 count=0;
        rc=accountListAllUsers(ids,ACC_USER_LIST_SIZE,&count);
        if(R_FAILED(rc) || count<0 || count>ACC_USER_LIST_SIZE)return {{},{Error::AccountService,rc}};
        for(int i=0;i<count;++i) {
            if(cancel())return {{},{Error::Cancelled}};
            Account a; a.key={{ids[i].uid[0],ids[i].uid[1]}};a.ordinal=i+1;
            if(!a.key.valid())return {{},{Error::InvalidData}};
            AccountProfile profile{};
            rc=accountGetProfile(&profile,ids[i]);
            if(R_SUCCEEDED(rc)) {
                auto release=finally([&]{accountProfileClose(&profile);});
                AccountProfileBase base{};
                rc=accountProfileGet(&profile,nullptr,&base);
                if(R_SUCCEEDED(rc))a.name.assign(base.nickname,strnlen(base.nickname,sizeof(base.nickname)));
            }
            a.profileWarning=R_FAILED(rc); result.value.push_back(std::move(a));
        }
        return result;
    }
    Status open(AccountKey key,const Cancel& cancel) override {
        close();
        if(cancel())return {Error::Cancelled};
        if(!key.valid())return {Error::InvalidData};
        FsSaveDataInfoReader reader{};
        auto rc=fsOpenSaveDataInfoReader(&reader,FsSaveDataSpaceId_User);
        if(R_FAILED(rc))return {Error::Metadata,rc};
        bool found=false;
        {
            auto release=finally([&]{fsSaveDataInfoReaderClose(&reader);});
            for(;;) {
                if(cancel())return {Error::Cancelled};
                FsSaveDataInfo info{};s64 count=0;
                rc=fsSaveDataInfoReaderRead(&reader,&info,1,&count);
                if(R_FAILED(rc) || count<0 || count>1)return {Error::Metadata,rc};
                if(!count)break;
                if(info.application_id==0x010051F0207B2000ULL &&
                   info.save_data_type==FsSaveDataType_Account &&
                   info.uid.uid[0]==key.words[0] && info.uid.uid[1]==key.words[1])found=true;
            }
        }
        if(!found)return {Error::NoSave};
        AccountUid uid{{key.words[0],key.words[1]}};
        rc=fsOpen_SaveDataReadOnly(&fs,0x010051F0207B2000ULL,uid);
        if(R_FAILED(rc))return {Error::Open,rc};
        opened=true;allowed.insert("/Player.sav");return {};
    }
    Result<std::vector<File>> enumerate(const Cancel& cancel) override {
        if(!opened)return {{},{Error::Open}};
        FsDirEntryType type{};
        auto rc=fsFsGetEntryType(&fs,"/Ugc",&type);
        if(R_FAILED(rc))return {{},{rc==0x202?Error::UgcMissing:Error::Enumeration,rc}};
        if(type!=FsDirEntryType_Dir)return {{},{Error::UgcMissing}};
        FsDir dir{};
        rc=fsFsOpenDirectory(&fs,"/Ugc",FsDirOpenMode_ReadFiles,&dir);
        if(R_FAILED(rc))return {{},{Error::Enumeration,rc}};
        auto release=finally([&]{fsDirClose(&dir);});
        Result<std::vector<File>> result;
        std::set<std::string> refs{"/Player.sav"};
        for(;;) {
            if(cancel())return {{},{Error::Cancelled}};
            FsDirectoryEntry item{};s64 count=0;
            rc=fsDirRead(&dir,&count,1,&item);
            if(R_FAILED(rc) || count<0 || count>1)return {{},{Error::Enumeration,rc}};
            if(!count)break;
            if(result.value.size()>=MaxRecords)return {{},{Error::Limit}};
            auto len=strnlen(item.name,sizeof(item.name));
            if(len==sizeof(item.name) || !len || item.file_size<0)return {{},{Error::InvalidData}};
            std::string name(item.name,len);
            if(name=="." || name==".." || name.find_first_of("/\\")!=std::string::npos)return {{},{Error::InvalidData}};
            result.value.push_back({name,uint64_t(item.file_size)});
            // Only enumerated compressed image references may be read.
            auto lower=name;for(auto& c:lower)if(c>='A' && c<='Z')c+='a'-'A';
            if(lower.size()>=3 && lower.substr(lower.size()-3)==".zs") refs.insert("/Ugc/"+name);
        }
        allowed=std::move(refs);return result;
    }
    Result<std::vector<uint8_t>> read(const std::string& ref,size_t limit,const Cancel& cancel) override {
        if(!opened || !allowed.count(ref))return {{},{Error::InvalidData}};
        if(cancel())return {{},{Error::Cancelled}};
        FsFile file{};
        auto rc=fsFsOpenFile(&fs,ref.c_str(),FsOpenMode_Read,&file);
        if(R_FAILED(rc))return {{},{Error::ShortRead,rc}};
        auto release=finally([&]{fsFileClose(&file);});
        s64 size=0;rc=fsFileGetSize(&file,&size);
        if(R_FAILED(rc) || size<0)return {{},{Error::ShortRead,rc}};
        limit=std::min(limit,ref=="/Player.sav"?MaxPlayerBytes:MaxImageBytes);
        if(uint64_t(size)>limit)return {{},{Error::Limit}};
        Result<std::vector<uint8_t>> result;result.value.resize(size);
        size_t off=0;
        while(off<size_t(size)) {
            if(cancel())return {{},{Error::Cancelled}};
            u64 actual=0;size_t requested=std::min(ReadChunk,size_t(size)-off);
            rc=fsFileRead(&file,off,result.value.data()+off,requested,FsReadOption_None,&actual);
            if(R_FAILED(rc) || !actual || actual>requested)return {{},{Error::ShortRead,rc}};
            off+=actual;
        }
        s64 finalSize=0;rc=fsFileGetSize(&file,&finalSize);
        if(R_FAILED(rc) || finalSize!=size)return {{},{Error::ShortRead,rc}};
        return result;
    }
    void close() override {allowed.clear();if(opened){fsFsClose(&fs);opened=false;}}
};
}
std::unique_ptr<ReadSource> createReadSource(){return std::make_unique<SwitchSource>();}
}
