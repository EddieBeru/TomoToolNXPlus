#include "profile_header.hpp"
#include "app_chrome.hpp"

namespace borealis_app {
ProfileHeader::ProfileHeader(const ProfileSelection& profile, std::function<void()> change)
    : Box(brls::Axis::ROW) {
    setWidth(350);
    setHeight(76);
    setShrink(0);
    setAlignItems(brls::AlignItems::CENTER);
    setPadding(6, 8, 6, 8);
    avatar = new brls::Image();
    avatar->setDimensions(56, 56);
    avatar->setShrink(0);
    avatar->setFreeTexture(true);
    avatar->setMarginRight(12);
    addView(avatar);
    placeholder = new brls::Label();
    placeholder->setText(brls::getStr("app/profile_no_photo"));
    placeholder->setSingleLine(false);
    placeholder->setFontSize(14);
    placeholder->setDimensions(56, 56);
    placeholder->setMarginRight(12);
    addView(placeholder);
    auto* text = new brls::Box(brls::Axis::COLUMN);
    text->setGrow(1);
    text->setShrink(1);
    nickname = new brls::Label();
    nickname->setFontSize(18);
    nickname->setLineHeight(1.1f);
    nickname->setSingleLine(false);
    text->addView(nickname);
    auto* caption = new brls::Label();
    caption->setFontSize(16);
    caption->setLineHeight(1.1f);
    caption->setSingleLine(false);
    caption->setText(brls::getStr(change ? "app/profile_change" : "app/profile_current"));
    text->addView(caption);
    addView(text);
    if (change) {
        setFocusable(true);
        registerClickAction([this, change](brls::View*) {
            brls::Application::giveFocus(this);
            change();
            return true;
        });
    }
    update(profile);
}
void ProfileHeader::update(const ProfileSelection& profile) {
    avatar->clear();
    bool selected = profile.state == ProfileSelectionState::Selected;
    if (selected && !profile.avatarJpeg.empty())
        avatar->setImageFromMem(profile.avatarJpeg.data(), int(profile.avatarJpeg.size()));
    bool hasPhoto = avatar->getTexture() != 0;
    avatar->setVisibility(hasPhoto ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    placeholder->setVisibility(hasPhoto ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    nickname->setText(!selected ? brls::getStr("app/profile_none") :
        (profile.account.name.empty() ? brls::getStr("app/browser_account") : profile.account.name));
}
ProfileHeader* addProfileHeader(brls::AppletFrame* frame, const ProfileSelection& profile,
    std::function<void()> change) {
    auto* header = static_cast<brls::Box*>(frame->getView("brls/applet_frame/hint_box"));
    auto* badge = new ProfileHeader(profile, std::move(change));
    header->addView(badge);
    hideStatusWidgets(frame);
    return badge;
}
}
