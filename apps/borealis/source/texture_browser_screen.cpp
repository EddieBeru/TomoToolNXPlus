#include "texture_browser_screen.hpp"
#include "exit_menu.hpp"
#include "profile_header.hpp"
#include "screen_activity.hpp"
#include "texture_preview_view.hpp"
#include "texture_settings_screen.hpp"
#include "texture_settings_store.h"
#include <algorithm>
namespace borealis_app {
namespace {
using namespace brls::literals;
using namespace TextureBrowser;
std::string tr(const std::string& key){return brls::getStr("app/browser_"+key);}
std::string errorMessage(Error e) {
    switch(e){
    case Error::AccountService:return brls::getStr("app/profile_not_selected");
    case Error::NoSave:return tr("no_save");
    case Error::Metadata:case Error::Open:return tr("access_error");
    case Error::UgcMissing:return tr("missing");
    case Error::Limit:return tr("limit");
    case Error::InvalidData:case Error::Unsupported:return tr("invalid");
    default:return tr("read_error");
    }
}
class Cell:public brls::DetailCell {
public:
    std::function<void()> focused;
    void onFocusGained() override {brls::DetailCell::onFocusGained();if(focused)focused();}
};
Cell* action(brls::Box* box,const std::string& text,std::function<void()> callback) {
    auto* cell=new Cell();cell->setText(text);
    cell->registerClickAction([cell,callback](brls::View*){
        brls::Application::giveFocus(cell);callback();return true;
    });
    box->addView(cell);return cell;
}
brls::Label* label(brls::Box* box,const std::string& text) {
    auto* result=new brls::Label();result->setSingleLine(false);result->setText(text);
    result->setMarginBottom(16);box->addView(result);return result;
}
class DetailScreen:public brls::AppletFrame {
    struct Life{DetailScreen* view;};
    std::shared_ptr<Life> life;
    std::shared_ptr<Session> session;
    Entry entry;
    brls::Label* status;
    TexturePreviewView* image;
    brls::DetailCell* retry;
    std::function<void()> returned;
    bool busy=false;
    std::function<void()> requestExit;
public:
    DetailScreen(std::shared_ptr<Session> session,Entry entry,std::function<void()> returned,std::function<void()> exit,const ProfileSelection& profile):
        AppletFrame(new brls::ScrollingFrame()),life(std::make_shared<Life>(Life{this})),
        session(std::move(session)),entry(std::move(entry)),returned(std::move(returned)),requestExit(std::move(exit)) {
        setTitle(tr("detail"));
        addProfileHeader(this,profile);
        auto* box=new brls::Box(brls::Axis::COLUMN);box->setPadding(20,28,20,28);
        static_cast<brls::ScrollingFrame*>(getContentView())->setContentView(box);
        label(box,this->entry.name);label(box,this->entry.id.stem);
        status=label(box,tr("loading"));
        image=new TexturePreviewView();box->addView(image);
        retry=action(box,tr("refresh"),[this]{load();});
        action(box,"app/exit_menu"_i18n,[this]{showExitMenu(requestExit);});
        registerAction("app/exit_menu"_i18n,brls::BUTTON_START,
            [this](brls::View*){showExitMenu(requestExit);return true;});
        action(box,"hints/back"_i18n,[this]{back();});
        registerAction("hints/back"_i18n,brls::BUTTON_B,[this](brls::View*){back();return true;});
        load();
    }
    ~DetailScreen() override {life->view=nullptr;session->cancel();}
private:
    void back(){session->cancel();returned();brls::Application::popActivity();}
    void load(){
        if(busy)return;
        busy=true;image->clear();status->setText(tr("loading"));
        retry->setFocusable(false);retry->setAlpha(0.45f);
        std::weak_ptr<Life> weak=life;auto selected=entry;
        session->submit([weak,selected](ReadSource& source,const Cancel& cancel){
            auto result=loadPreview(source,selected,cancel);
            return [weak,result=std::move(result)]{
                auto l=weak.lock();if(!l || !l->view)return;
                auto* self=l->view;self->busy=false;
                self->retry->setFocusable(true);self->retry->setAlpha(1);
                if(!result){self->status->setText(errorMessage(result.status.error));return;}
                if(!self->image->show(result.value)){self->status->setText(tr("preview_error"));return;}
                std::string message=tr(result.value.canvas?"canvas":"main")+" · "+
                    std::to_string(result.value.width)+" × "+std::to_string(result.value.height);
                if(result.value.fallback)message+="\n"+tr("fallback");
                self->status->setText(message);
            };
        });
    }
};
}
TextureBrowserScreen::TextureBrowserScreen(std::shared_ptr<Session> session,std::function<void()> tabs,std::function<void()> exit):
    life(std::make_shared<Life>(Life{this})),session(std::move(session)),requestExit(std::move(exit)) {
    list=new brls::Box(brls::Axis::COLUMN);
    list->setPadding(24,28,24,28);setContentView(list);setGrow(1);
    auto* header=new brls::Header();header->setTitle("app/textures"_i18n);list->addView(header);
    label(list,tr("note"));
    refreshControl=action(list,tr("refresh"),[this]{refresh();});
    settingsControl=action(list,"app/settings"_i18n,[this]{
        model.context.scroll=getContentOffsetY();
        brls::Application::pushActivity(new ScreenActivity(
            createTextureSettingsScreen(TextureSettings::ConfigPath,requestExit,profile)),
            brls::TransitionAnimation::NONE);
    });
    status=label(list,tr("choose_global"));
    entries=new brls::Box(brls::Axis::COLUMN);list->addView(entries);
    action(list,"app/tabs"_i18n,tabs);
    registerAction("app/tabs_hint"_i18n,brls::BUTTON_B,[tabs](brls::View*){tabs();return true;});
}
TextureBrowserScreen::~TextureBrowserScreen(){life->view=nullptr;session->cancel();}
void TextureBrowserScreen::setProfile(const ProfileSelection& selection){
    if(selection.state!=ProfileSelectionState::Selected || !selection.account.key.valid())return;
    profile=selection;
    if(model.hasAccount && model.context.account.key==selection.account.key){
        model.context.account=selection.account;
        return;
    }
    session->cancel();
    model.select(selection.account);initialLoad=true;dirty=true;
}
void TextureBrowserScreen::activateBrowser(){
    active=true;
    if(initialLoad){initialLoad=false;refresh(false);return;}
    if(model.resume())refresh(false);else if(dirty)render();else restore();
}
void TextureBrowserScreen::deactivateBrowser(){
    if(!active)return;
    active=false;model.context.scroll=getContentOffsetY();
    if(model.state==State::CatalogLoading){model.suspend();session->cancel();}
    else if(model.state==State::AccountsLoading){model.cancelChooser();session->cancel();}
}
void TextureBrowserScreen::willAppear(bool resetState){
    brls::ScrollingFrame::willAppear(resetState);activateBrowser();
}
void TextureBrowserScreen::willDisappear(bool resetState){
    deactivateBrowser();brls::ScrollingFrame::willDisappear(resetState);
}
void TextureBrowserScreen::refresh(bool begin){
    if(!model.hasAccount)return;
    if(begin && model.state==State::CatalogLoading)return;
    if(begin)model.loading();
    render();
    std::weak_ptr<Life> weak=life;auto account=model.context.account.key;auto generation=model.generation;
    session->submit([weak,account,generation](ReadSource& source,const Cancel& cancel){
        auto result=loadCatalog(source,account,cancel);
        return [weak,account,generation,result=std::move(result)]() mutable {
            auto locked=weak.lock();if(!locked || !locked->view)return;
            auto* self=locked->view;
            if(self->model.generation!=generation || self->model.context.account.key!=account)return;
            self->model.accept(std::move(result));
            self->dirty=true;
            if(self->active)self->render();
        };
    });
}
void TextureBrowserScreen::render(){
    if(!active){dirty=true;return;}
    dirty=false;
    bool loading=model.state==State::CatalogLoading || model.state==State::AccountsLoading;
    refreshControl->setFocusable(model.hasAccount && !loading);
    refreshControl->setAlpha(model.hasAccount && !loading?1.0f:0.45f);
    brls::Application::giveFocus(settingsControl);
    pendingFocus=nullptr;
    entries->clearViews();
    if(loading){status->setText(tr("loading"));return;}
    if(model.state==State::CatalogError){status->setText(errorMessage(model.status.error));return;}
    if(model.state==State::Empty){status->setText(tr("empty"));return;}
    if(model.state!=State::Ready){status->setText(tr("choose_global"));return;}
    status->setText(model.catalog.namesWarning?tr("names_warning"):tr("readonly"));
    auto selected=model.context.selectedStem;
    for(size_t i=0;i<model.catalog.entries.size();++i){
        auto entry=model.catalog.entries[i];
        auto* cell=action(entries,entry.name,[this,entry,i]{
            model.context.selectedStem=entry.id.stem;model.context.ordinal=i;
            model.context.scroll=getContentOffsetY();openEntry(entry);
        });
        cell->setDetailText(entry.id.stem);
        cell->focused=[this,i]{
            model.context.ordinal=i;model.context.selectedStem=model.catalog.entries[i].id.stem;
        };
        if(entry.id.stem==selected){
            pendingFocus=cell;
            entries->setLastFocusedView(cell);
            list->setLastFocusedView(entries);
        }
    }
    restore();
}
void TextureBrowserScreen::restore(){restorePending=true;invalidate();}
void TextureBrowserScreen::onLayout(){
    brls::ScrollingFrame::onLayout();
    if(restorePending && active){
        restorePending=false;
        if(pendingFocus){brls::Application::giveFocus(pendingFocus);pendingFocus=nullptr;}
        auto offset=std::max(0.0f,model.context.scroll);
        offset=std::min(offset,std::max(0.0f,list->getHeight()-getHeight()));
        setContentOffsetY(offset,false);
    }
}
void TextureBrowserScreen::openEntry(const Entry& entry){
    std::weak_ptr<Life> weak=life;
    brls::Application::pushActivity(new ScreenActivity(new DetailScreen(session,entry,[weak]{
        auto l=weak.lock();if(l && l->view)l->view->restore();
    },requestExit,profile)));
}
}
