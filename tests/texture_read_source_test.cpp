#include "texture_read_source.hpp"
#include <switch.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
static std::vector<std::string> calls;
static Result failure=0;
static bool metadataDone=false,dirDone=false, noSave=false, missing=false, interrupted=false;
static Result metadataFailure=0, readOnlyFailure=0, profileFailure=0;
static int records=1, recordIndex=0;
static s32 accountCount=1;
static s64 fileSize=131072;
static bool shortRead=false, changeSize=false;
static int sizeCalls=0;
static std::vector<uint8_t> bytes(131072,7);
Result accountInitialize(AccountServiceType){return failure;}
void accountExit(){calls.push_back("account-close");}
Result accountListAllUsers(AccountUid* a,s32,s32* n){*n=accountCount;a[0]={{1,0}};return failure;}
Result accountGetProfile(AccountProfile*,AccountUid){return profileFailure?profileFailure:failure;}
Result accountProfileGet(AccountProfile*,AccountUserData*,AccountProfileBase* p){std::memset(p->nickname,'X',32);return failure;}
void accountProfileClose(AccountProfile*){calls.push_back("profile-close");}
Result fsOpenSaveDataInfoReader(FsSaveDataInfoReader*,FsSaveDataSpaceId){metadataDone=false;return failure;}
Result fsSaveDataInfoReaderRead(FsSaveDataInfoReader*,FsSaveDataInfo* p,size_t,s64* n){
    *n=metadataDone?0:1; metadataDone=true;p->application_id=noSave?0:0x010051F0207B2000ULL;p->uid={{1,0}};
    p->save_data_type=FsSaveDataType_Account;return metadataFailure?metadataFailure:failure;
}
void fsSaveDataInfoReaderClose(FsSaveDataInfoReader*){calls.push_back("metadata-close");}
Result fsOpen_SaveDataReadOnly(FsFileSystem*,u64 id,AccountUid uid){assert(id==0x010051F0207B2000ULL && uid.uid[0]==1);calls.push_back("open-readonly");return readOnlyFailure?readOnlyFailure:failure;}
void fsFsClose(FsFileSystem*){calls.push_back("fs-close");}
Result fsFsGetEntryType(FsFileSystem*,const char*,FsDirEntryType* t){*t=FsDirEntryType_Dir;return missing?0x202:failure;}
Result fsFsOpenDirectory(FsFileSystem*,const char* p,u32 mode,FsDir*){assert(std::string(p)=="/Ugc" && mode==FsDirOpenMode_ReadFiles);dirDone=false;recordIndex=0;return failure;}
Result fsDirRead(FsDir*,s64* n,size_t,FsDirectoryEntry* e){if(interrupted && dirDone)return 4;*n=recordIndex++<records?1:0;dirDone=true;std::strcpy(e->name,"UgcFood000.ugctex.zs");e->type=FsDirEntryType_File;e->file_size=fileSize;return failure;}
void fsDirClose(FsDir*){calls.push_back("dir-close");}
Result fsFsOpenFile(FsFileSystem*,const char*,u32 mode,FsFile*){assert(mode==FsOpenMode_Read);calls.push_back("file-open-read");sizeCalls=0;return failure;}
Result fsFileGetSize(FsFile*,s64* n){*n=fileSize+(changeSize && sizeCalls++?1:0);return failure;}
Result fsFileRead(FsFile*,s64 off,void* out,u64 n,u32,u64* actual){
    assert(n<=65536);*actual=shortRead?0:n; if(*actual)std::memcpy(out,bytes.data()+off,n);return failure;
}
void fsFileClose(FsFile*){calls.push_back("file-close");}
int main(){
    using namespace TextureBrowser;
    auto source=createReadSource(); Cancel no=[] {return false;};
    failure=4;assert(source->accounts(no).status.error==Error::AccountService);failure=0;
    auto accounts=source->accounts(no);assert(accounts && accounts.value.size()==1 && accounts.value[0].name.size()==32);
    accountCount=0;assert(source->accounts(no).value.empty());accountCount=1;
    profileFailure=4;auto fallback=source->accounts(no);assert(fallback && fallback.value[0].profileWarning);profileFailure=0;
    readOnlyFailure=4;assert(source->open({{1,0}},no).error==Error::Open);readOnlyFailure=0;
    assert(source->open({{1,0}},no));
    records=8193;assert(source->enumerate(no).status.error==Error::Limit);records=1;
    assert(source->enumerate(no));
    auto r=source->read("/Ugc/UgcFood000.ugctex.zs",MaxImageBytes,no);assert(r && r.value==bytes);
    assert(!source->read("/Ugc/../Player.sav",MaxImageBytes,no));
    shortRead=true;assert(!source->read("/Player.sav",MaxPlayerBytes,no));shortRead=false;
    changeSize=true;assert(!source->read("/Player.sav",MaxPlayerBytes,no));changeSize=false;
    fileSize=8*1024*1024+1;assert(source->read("/Player.sav",MaxPlayerBytes,no).status.error==Error::Limit);
    fileSize=-1;assert(!source->read("/Player.sav",MaxPlayerBytes,no));fileSize=131072;
    failure=4;assert(!source->enumerate(no));failure=0;
    interrupted=true;assert(!source->enumerate(no));interrupted=false;
    missing=true;assert(source->enumerate(no).status.error==Error::UgcMissing);missing=false;
    noSave=true;assert(source->open({{1,0}},no).error==Error::NoSave);noSave=false;
    metadataFailure=4;assert(source->open({{1,0}},no).error==Error::Metadata);metadataFailure=0;
    assert(source->open({{1,0}},no));
    assert(source->enumerate(no));
    source->close(); assert(calls.back()=="fs-close");
    assert(!source->open({{1,0}},[]{return true;}));
    std::cout<<"read-only source: OK\n";
}
