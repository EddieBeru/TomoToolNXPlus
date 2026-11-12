#include "navigation.hpp"
#include <cassert>
#include <cstdio>
#include <memory>

static bool press(brls::View* focus,brls::ControllerButton button){
    for(auto* view=focus;view;view=view->getParent()){
        const auto found=view->actions.find(button);
        if(found!=view->actions.end() && found->second.callback(view))return true;
    }
    return false;
}
int main(){
    using namespace borealis_app::navigation;
    brls::View rootView,header,entry,detail,detailEntry,modal,confirm;
    header.parent=&rootView;entry.parent=&rootView;
    detailEntry.parent=&detail;confirm.parent=&modal;
    int rootMenu=0,detailMenu=0,backCount=0,tab=0;
    root(&rootView);
    saveExit(&rootView,[&]{++rootMenu;});
    tabs(&rootView,[&](int direction){tab+=direction;});
    back(&detail,[&]{++backCount;});
    saveExit(&detail,[&]{++detailMenu;});
    assert(rootView.actions.size()==4);
    assert(detail.actions.size()==2);
    assert(rootView.actions.at(brls::BUTTON_B).hidden);
    assert(!rootView.actions.at(brls::BUTTON_START).hidden);
    assert(rootView.actions.at(brls::BUTTON_START).label=="app/exit_menu");
    for(const auto& action:rootView.actions)assert(!action.second.repeating);
    assert(press(&entry,brls::BUTTON_START));
    assert(press(&header,brls::BUTTON_START));
    assert(rootMenu==2);
    assert(press(&entry,brls::BUTTON_B));
    assert(press(&entry,brls::BUTTON_LT));assert(tab==-1);
    assert(press(&header,brls::BUTTON_RT));assert(tab==0);
    assert(press(&detailEntry,brls::BUTTON_START));
    assert(detailMenu==1 && rootMenu==2);
    assert(press(&detailEntry,brls::BUTTON_B));assert(backCount==1);
    assert(!press(&confirm,brls::BUTTON_START));
    assert(!press(&confirm,brls::BUTTON_LT));
    assert(detailMenu==1 && rootMenu==2);

    initialize();
    std::unique_ptr<brls::View> footer(brls::Application::factories.at("brls:BottomBar")());
    auto* bar=dynamic_cast<brls::BottomBar*>(footer.get());assert(bar);
    for(auto* id:{"brls/battery","brls/wireless","brls/hints/time"})
        assert(bar->getView(id)->visibility==brls::Visibility::GONE);
    std::puts("Borealis actual navigation action registration and footer policy: OK");
}
