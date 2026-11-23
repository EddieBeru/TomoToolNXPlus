#pragma once
#include <borealis.hpp>
#include "mii_roster.h"
#include "profile_selector.hpp"
#include "texture_browser_session.hpp"

namespace borealis_app {

class MiiBrowserScreen : public brls::ScrollingFrame {
public:
    MiiBrowserScreen(
        std::shared_ptr<TextureBrowser::Session> session,
        std::function<void()> tabs,
        std::function<void()> exit);
    ~MiiBrowserScreen() override;

    void onLayout() override;
    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;

    void setProfile(const ProfileSelection& selection);
    void activateBrowser();
    void deactivateBrowser();

private:
    struct Life {
        MiiBrowserScreen* view;
    };

    std::shared_ptr<Life> life;
    std::shared_ptr<TextureBrowser::Session> session;
    MiiRoster::MiiBrowserModel model;
    std::shared_ptr<const MiiRoster::MiiRosterSummary> snapshot;
    ProfileSelection profile;
    std::function<void()> focusTabs;
    std::function<void()> requestExit;

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
