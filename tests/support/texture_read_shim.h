#pragma once
#include "texture_read_source.hpp"
#include <switch.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "texture_browser_fixtures.h"
#include <algorithm>
static std::vector<std::string> calls;
static Result failure=0;
static std::map<uint64_t, Fixtures::Files> saves;
static uint64_t activeAccount=1;
static std::string activeRef;
static size_t partialBytes=65536;
static int filesOpen=0, dirsOpen=0;
static bool filesystemOpen=false;
static std::function<void()> beforeRead;
static int metadataIndex=0;
static bool metadataDone=false,dirDone=false, noSave=false, missing=false, interrupted=false;
static Result metadataFailure=0, readOnlyFailure=0, profileFailure=0;
static int records=1, recordIndex=0;
static s32 accountCount=1;
static s64 fileSize=131072;
static bool shortRead=false, changeSize=false;
static int sizeCalls=0;
static Result sizeFailure=0, readFailure=0;
static std::vector<uint8_t> bytes(131072,7);
Result accountInitialize(AccountServiceType){return failure;}
void accountExit(){calls.push_back("account-close");}
Result accountListAllUsers(AccountUid* a,s32,s32* n){*n=accountCount;a[0]={{1,0}};return failure;}
Result accountGetProfile(AccountProfile*,AccountUid){return profileFailure?profileFailure:failure;}
Result accountProfileGet(AccountProfile*,AccountUserData*,AccountProfileBase* p){std::memset(p->nickname,'X',32);return failure;}
void accountProfileClose(AccountProfile*){calls.push_back("profile-close");}
Result fsOpenSaveDataInfoReader(FsSaveDataInfoReader*,FsSaveDataSpaceId){metadataDone=false;metadataIndex=0;return failure;}
Result fsSaveDataInfoReaderRead(FsSaveDataInfoReader*,FsSaveDataInfo* p,size_t,s64* n){
    *n=saves.empty()?(metadataDone?0:1):(metadataIndex<2?1:0); metadataDone=true;p->application_id=noSave?0:0x010051F0207B2000ULL;p->uid={{saves.empty()?1u:uint64_t(++metadataIndex),0}};
    p->save_data_type=FsSaveDataType_Account;return metadataFailure?metadataFailure:failure;
}
void fsSaveDataInfoReaderClose(FsSaveDataInfoReader*){calls.push_back("metadata-close");}
Result fsOpen_SaveDataReadOnly(FsFileSystem*,u64 id,AccountUid uid){assert(id==0x010051F0207B2000ULL);activeAccount=uid.uid[0];calls.push_back("open-readonly");filesystemOpen=!(readOnlyFailure?readOnlyFailure:failure);return readOnlyFailure?readOnlyFailure:failure;}
void fsFsClose(FsFileSystem*){assert(filesOpen==0 && dirsOpen==0);filesystemOpen=false;calls.push_back("fs-close");}
Result fsFsGetEntryType(FsFileSystem*,const char*,FsDirEntryType* t){*t=FsDirEntryType_Dir;return missing?0x202:failure;}
Result fsFsOpenDirectory(FsFileSystem*,const char* p,u32 mode,FsDir*){assert(std::string(p)=="/Ugc" && mode==FsDirOpenMode_ReadFiles);dirDone=false;recordIndex=0;if(!failure)++dirsOpen;return failure;}
Result fsDirRead(FsDir*,s64* n,size_t,FsDirectoryEntry* e){
    if(interrupted && dirDone)return 4;
    dirDone=true;
    if(!saves.empty()) {
        std::vector<std::string> names;
        for(const auto& f:saves.at(activeAccount))if(f.first.rfind("/Ugc/",0)==0)names.push_back(f.first.substr(5));
        *n=size_t(recordIndex)<names.size()?1:0;
        if(*n){auto name=names[recordIndex++];std::strcpy(e->name,name.c_str());e->file_size=saves.at(activeAccount).at("/Ugc/"+name).size();}
    } else {*n=recordIndex++<records?1:0;std::strcpy(e->name,"UgcFood000.ugctex.zs");e->file_size=fileSize;}
    e->type=FsDirEntryType_File;return failure;
}
void fsDirClose(FsDir*){assert(filesystemOpen && dirsOpen==1);--dirsOpen;calls.push_back("dir-close");}
Result fsFsOpenFile(FsFileSystem*,const char* path,u32 mode,FsFile*){assert(mode==FsOpenMode_Read);calls.push_back("file-open-read");sizeCalls=0;activeRef=path;if(!saves.empty() && !saves.at(activeAccount).count(activeRef))return 4;if(!failure)++filesOpen;return failure;}
Result fsFileGetSize(FsFile*,s64* n){*n=(saves.empty()?fileSize:s64(saves.at(activeAccount).at(activeRef).size()))+(changeSize && sizeCalls++?1:0);return sizeFailure?sizeFailure:failure;}
Result fsFileRead(FsFile*,s64 off,void* out,u64 n,u32,u64* actual){
    assert(n<=65536);if(readFailure)return readFailure;if(beforeRead)beforeRead();*actual=shortRead?0:std::min<u64>(n,partialBytes);if(*actual){const auto& data=saves.empty()?bytes:saves.at(activeAccount).at(activeRef);assert(off+*actual<=data.size());std::memcpy(out,data.data()+off,*actual);}return failure;
}
void fsFileClose(FsFile*){assert(filesystemOpen && filesOpen==1);--filesOpen;calls.push_back("file-close");}

static Result mutation(const char* operation) {
    calls.push_back(operation);assert(false && "save mutation attempted");return 4;
}
Result fsFileWrite(FsFile*,s64,const void*,u64,u32){return mutation("write");}
Result fsFsCommit(FsFileSystem*){return mutation("commit");}
Result fsFsCreateFile(FsFileSystem*,const char*,s64,u32){return mutation("create");}
Result fsFsDeleteFile(FsFileSystem*,const char*){return mutation("delete");}
Result fsFsRenameFile(FsFileSystem*,const char*,const char*){return mutation("rename");}
