#pragma once

#include "texture_settings.h"
#include <string>

namespace TextureSettings {

// Flat config.ini tokens shared with the existing Switch application.
bool DecodeConfig(const std::string& text, State& state);
bool PatchConfig(const std::string& original, const State& before,
                 const State& after, std::string& output);

} // namespace TextureSettings
