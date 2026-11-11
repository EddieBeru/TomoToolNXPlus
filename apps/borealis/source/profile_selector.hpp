#pragma once
#include "texture_browser.h"

namespace borealis_app {
enum class ProfileSelectionState { Selected, Skipped, Failed };
struct ProfileSelection {
    ProfileSelectionState state = ProfileSelectionState::Skipped;
    TextureBrowser::Account account;
    uint32_t platformResult = 0;
    std::vector<uint8_t> avatarJpeg;
};
// The native library applet must run on the application thread, not the read worker.
ProfileSelection selectNativeProfile();
}
