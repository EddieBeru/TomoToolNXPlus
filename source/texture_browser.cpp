#include "texture_browser.h"
#include "ugc_scanner.h"
#include "ugc_names.h"
#include <algorithm>
namespace TextureBrowser {
void Model::chooseAccounts(){beforeChooser=state;state=State::AccountsLoading;status={};}
void Model::cancelChooser(){state=beforeChooser;}
void Model::select(Account account) {
    context={};context.account=std::move(account);hasAccount=true;catalog={};loading();
}
void Model::loading(){++generation;state=State::CatalogLoading;status={};}
void Model::suspend(){if(state==State::CatalogLoading){++generation;state=State::CatalogSuspended;}}
bool Model::resume(){if(state!=State::CatalogSuspended)return false;loading();return true;}
void Model::accept(Result<Catalog> result) {
    status=result.status;
    if(!result){catalog={};state=State::CatalogError;return;}
    catalog=std::move(result.value);state=catalog.entries.empty()?State::Empty:State::Ready;
    size_t index=std::min(context.ordinal,catalog.entries.empty()?0:catalog.entries.size()-1);
    for(size_t i=0;i<catalog.entries.size();++i)
        if(catalog.entries[i].id.stem==context.selectedStem){index=i;break;}
    context.ordinal=index;
    if(catalog.entries.empty()){context.selectedStem.clear();context.focusTarget="refresh";}
    else {context.selectedStem=catalog.entries[index].id.stem;context.focusTarget=context.selectedStem;}
}
Result<Catalog> loadCatalog(ReadSource& source,AccountKey account,const Cancel& cancel) {
    auto status=source.open(account,cancel);if(!status)return {{},status};
    auto files=source.enumerate(cancel);if(!files)return {{},files.status};
    std::vector<std::string> names;
    for(const auto& f:files.value)names.push_back(f.name);
    auto associated=UgcScanner::Associate(names);if(!associated.ok)return {{},{associated.limit?Error::Limit:Error::InvalidData}};
    auto player=source.read("/Player.sav",MaxPlayerBytes,cancel);
    UgcNames::Names labels;
    if(player) labels=UgcNames::parse(player.value);
    else labels.warning=true;
    if(cancel())return {{},{Error::Cancelled}};
    Catalog catalog;catalog.namesWarning=labels.warning;
    for(auto& entry:associated.entries) {
        Entry e;e.id={account,entry.stem};e.name=entry.stem;
        auto name=labels.values.find(entry.stem);
        if(name!=labels.values.end())e.name=name->second;
        e.mainRef="/Ugc/"+entry.ugctexPath;
        if(!entry.canvasPath.empty())e.canvasRef="/Ugc/"+entry.canvasPath;
        if(!entry.thumbPath.empty())e.thumbRef="/Ugc/"+entry.thumbPath;
        e.associationWarning=associated.warning;catalog.entries.push_back(std::move(e));
    }
    return {std::move(catalog),{}};
}
}
