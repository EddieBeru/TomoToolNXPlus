#pragma once
#include <borealis.hpp>
#include "profile_selector.hpp"

namespace borealis_app {
class ProfileHeader : public brls::Box {
public:
    ProfileHeader(const ProfileSelection& profile, std::function<void()> change = {});
    void update(const ProfileSelection& profile);
private:
    brls::Image* avatar;
    brls::Label* placeholder;
    brls::Label* nickname;
};
// Child frames show the same identity without changing the account behind an editor.
ProfileHeader* addProfileHeader(brls::AppletFrame* frame, const ProfileSelection& profile,
    std::function<void()> change = {});
}
