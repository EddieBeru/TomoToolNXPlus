#pragma once
#include <cstdint>
#include <cstddef>
using u64=uint64_t;using u32=uint32_t;using s64=int64_t;using s32=int32_t;
using Result=u32;
#define R_FAILED(r) ((r)!=0)
#define R_SUCCEEDED(r) ((r)==0)
constexpr int ACC_USER_LIST_SIZE=8;
enum AccountServiceType {AccountServiceType_Application,AccountServiceType_System};
struct AccountUid {u64 uid[2];};
struct AccountProfile {int id;};
struct AccountUserData {};
struct AccountProfileBase {AccountUid uid;u64 lastEditTimestamp;char nickname[0x20];};
struct FsFileSystem {int id;};
struct FsFile {int id;};
struct FsDir {int id;};
struct FsSaveDataInfoReader {int id;};
enum FsSaveDataSpaceId {FsSaveDataSpaceId_User};
enum {FsSaveDataType_Account=1,FsOpenMode_Read=1,FsDirOpenMode_ReadFiles=2,FsReadOption_None=0};
enum FsDirEntryType {FsDirEntryType_Dir,FsDirEntryType_File};
struct FsDirectoryEntry {char name[0x301];int8_t type;s64 file_size;};
struct FsSaveDataInfo {u64 application_id;AccountUid uid;uint8_t save_data_type;};
Result accountInitialize(AccountServiceType);
void accountExit();
Result accountListAllUsers(AccountUid*,s32,s32*);
Result accountGetProfile(AccountProfile*,AccountUid);
Result accountProfileGet(AccountProfile*,AccountUserData*,AccountProfileBase*);
void accountProfileClose(AccountProfile*);
Result fsOpenSaveDataInfoReader(FsSaveDataInfoReader*,FsSaveDataSpaceId);
Result fsSaveDataInfoReaderRead(FsSaveDataInfoReader*,FsSaveDataInfo*,size_t,s64*);
void fsSaveDataInfoReaderClose(FsSaveDataInfoReader*);
Result fsOpen_SaveDataReadOnly(FsFileSystem*,u64,AccountUid);
void fsFsClose(FsFileSystem*);
Result fsFsGetEntryType(FsFileSystem*,const char*,FsDirEntryType*);
Result fsFsOpenDirectory(FsFileSystem*,const char*,u32,FsDir*);
Result fsDirRead(FsDir*,s64*,size_t,FsDirectoryEntry*);
void fsDirClose(FsDir*);
Result fsFsOpenFile(FsFileSystem*,const char*,u32,FsFile*);
Result fsFileGetSize(FsFile*,s64*);
Result fsFileRead(FsFile*,s64,void*,u64,u32,u64*);
void fsFileClose(FsFile*);
