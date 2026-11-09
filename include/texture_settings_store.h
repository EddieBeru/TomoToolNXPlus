#pragma once

#include "texture_settings.h"
#include <string>
#include <utility>

namespace TextureSettings {

constexpr const char* ConfigPath = "/switch/TomoToolNX/config.ini";
enum class ConfigError { None, Read, Invalid, Conflict, Backup, Write, Verify };
struct ConfigResult {
    ConfigError error = ConfigError::None;
    bool mayHaveChanged = false;
    std::string backupPath;
    explicit operator bool() const { return error == ConfigError::None; }
};

// SD configuration only. No game filesystem, user account or renderer dependency.
class ConfigStore {
public:
    explicit ConfigStore(std::string path) : path_(std::move(path)) {}
    ConfigResult Load(State& state);
    ConfigResult Save(const State& state);
    bool Dirty(const State& state) const;
    bool Exists() const { return exists_; }
private:
    std::string path_, original_, recoveryBackup_;
    State baseline_;
    bool loaded_ = false, exists_ = false;
    bool recoveryRequired_ = false;
};
} // namespace TextureSettings
