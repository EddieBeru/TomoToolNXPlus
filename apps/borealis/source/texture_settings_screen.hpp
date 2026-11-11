#pragma once

#include <borealis.hpp>
#include <string>
#include "profile_selector.hpp"

namespace borealis_app {
brls::View* createTextureSettingsScreen(const std::string& configPath, std::function<void()> requestExit = {}, const ProfileSelection& profile = {});
}
