#include "support/texture_read_shim.h"
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
    partialBytes=997;
    r=source->read("/Ugc/UgcFood000.ugctex.zs",MaxImageBytes,no);assert(r && r.value==bytes);
    partialBytes=65536;
    sizeFailure=4;assert(!source->read("/Player.sav",MaxPlayerBytes,no));assert(filesOpen==0);sizeFailure=0;
    readFailure=4;assert(!source->read("/Player.sav",MaxPlayerBytes,no));assert(filesOpen==0);readFailure=0;
    shortRead=true;assert(!source->read("/Player.sav",MaxPlayerBytes,no));shortRead=false;assert(filesOpen==0);
    changeSize=true;assert(!source->read("/Player.sav",MaxPlayerBytes,no));changeSize=false;assert(filesOpen==0);
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
