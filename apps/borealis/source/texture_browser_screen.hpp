#pragma once
#include <borealis.hpp>
#include "texture_browser_session.hpp"
namespace borealis_app {
class TextureBrowserScreen : public brls::ScrollingFrame {
public:
    TextureBrowserScreen(std::shared_ptr<TextureBrowser::Session> session,std::function<void()> tabs);
    ~TextureBrowserScreen() override;
    void onLayout() override;
    void willAppear(bool resetState=false) override;
    void willDisappear(bool resetState=false) override;
    void setInitialProfile(TextureBrowser::Account account);
    void activateBrowser();
    void deactivateBrowser();
private:
    struct Life {TextureBrowserScreen* view;};
    std::shared_ptr<Life> life;
    std::shared_ptr<TextureBrowser::Session> session;
    TextureBrowser::Model model;
    brls::Box* list=nullptr;
    brls::Box* entries=nullptr;
    brls::Label* status=nullptr;
    brls::DetailCell* accountControl=nullptr;
    brls::DetailCell* refreshControl=nullptr;
    bool active=false;
    bool restorePending=false;
    bool dirty=true;
    bool initialLoad=false;
    brls::View* pendingFocus=nullptr;
    void choose();
    void refresh(bool begin=true);
    void render();
    void openEntry(const TextureBrowser::Entry& entry);
    void restore();
};
}
