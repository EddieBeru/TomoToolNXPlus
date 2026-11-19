#include "texture_browser.h"
#include "support/texture_browser_fixtures.h"
#include <cassert>
#include <iostream>
int main(){
    using namespace TextureBrowser;
    Fixtures::Source source;Cancel no=[]{return false;};
    for(size_t count:{0u,1u,100u}) {
        source.files=Fixtures::collection(count);source.files["/Player.sav"]=Fixtures::names();
        auto result=loadCatalog(source,{{1,0}},no);
        assert(result && result.value.entries.size()==count);
        if(count)assert(result.value.entries[0].name=="A");
    }
    Model m;m.select({{{1,0}},"A",1,false});assert(m.state==State::CatalogLoading);
    m.context.selectedStem="UgcFood000";m.context.scroll=23;
    m.suspend();assert(m.state==State::CatalogSuspended);
    auto gen=m.generation;assert(m.resume() && m.generation>gen && !m.resume());
    assert(m.context.scroll==23);
    auto result=loadCatalog(source,{{1,0}},no);m.accept(std::move(result));
    m.chooseAccounts();m.cancelChooser();assert(m.state==State::Ready);
    auto original=m.context.selectedStem;
    m.chooseAccounts();m.cancelChooser();assert(m.context.selectedStem==original);
    m.context.ordinal=99;m.context.selectedStem="gone";
    auto shorter=loadCatalog(source,{{1,0}},no);shorter.value.entries.resize(2);
    m.accept(std::move(shorter));assert(m.context.ordinal==1);
    m.accept({{}, {}});assert(m.state==State::Empty && m.context.focusTarget=="refresh");
    auto other=loadCatalog(source,{{2,0}},no);
    assert(other.value.entries[0].id.account.words[0]==2);
    assert(!loadCatalog(source,{{1,0}},[]{return true;}));

    source.accountFiles[1]=Fixtures::collection(1);
    source.accountFiles[1]["/Player.sav"]=Fixtures::names();
    source.accountFiles[2]=Fixtures::collection(1,17);
    source.accountFiles[2]["/Player.sav"]=Fixtures::names();
    source.accountFiles[2]["/Player.sav"][0x44]='B';
    auto snapshot=source.accountFiles;
    auto a=loadCatalog(source,{{1,0}},no),b=loadCatalog(source,{{2,0}},no);
    assert(a.value.entries[0].name=="A" && b.value.entries[0].name=="B");
    source.failure={Error::Enumeration};
    assert(!loadCatalog(source,{{1,0}},no));source.failure={};
    assert(snapshot==source.accountFiles);
    for(const auto& op:source.operations)assert(op=="read" || op=="open-read" || op=="close");
    source.files=Fixtures::collection(1);
    source.files["/Ugc/UgcFood000_Thumb_ugctex.zs"]={};
    source.files["/Ugc/ugcfood000_THUMB_UGCTEX.ZS"]={};
    source.accountFiles.clear();
    auto ambiguous=loadCatalog(source,{{1,0}},no);
    assert(ambiguous && ambiguous.value.namesWarning);
    assert(ambiguous.value.entries.size()==1 && ambiguous.value.entries[0].associationWarning);
    assert(ambiguous.value.entries[0].thumbRef.empty() && !ambiguous.value.entries[0].mainRef.empty());
    std::cout<<"texture browser: OK\n";
}
