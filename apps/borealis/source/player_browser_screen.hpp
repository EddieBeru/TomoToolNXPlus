#pragma once
#include <borealis.hpp>
#include "player_profile.h"
#include "profile_selector.hpp"
#include "texture_browser_session.hpp"

namespace borealis_app {

class PlayerBrowserScreen : public brls::ScrollingFrame {
public:
    PlayerBrowserScreen(
        std::shared_ptr<TextureBrowser::Session> session,
        std::function<void()> tabs);
    ~PlayerBrowserScreen() override;

    void onLayout() override;
    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;

    void setProfile(const ProfileSelection& selection);
    void activateBrowser();
    void deactivateBrowser();

private:
    struct Life {
        PlayerBrowserScreen* view;
    };

    std::shared_ptr<Life> life;
    std::shared_ptr<TextureBrowser::Session> session;
    PlayerProfile::PlayerBrowserModel model;
    ProfileSelection profile;
    std::function<void()> focusTabs;

    brls::Box* list = nullptr;
    brls::Box* entries = nullptr;
    brls::Label* status = nullptr;
    brls::DetailCell* refreshControl = nullptr;
    brls::DetailCell* tabsControl = nullptr;

    bool active = false;
    bool restorePending = false;
    bool dirty = true;
    bool initialLoad = false;
    brls::View* pendingFocus = nullptr;

    void refresh(bool begin = true);
    void render();
    void restore();
};

} // namespace borealis_app
