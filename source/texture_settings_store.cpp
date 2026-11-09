#include "texture_settings_store.h"
#include "texture_settings_config.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace TextureSettings {
namespace {
constexpr size_t MaxConfigSize = 64 * 1024;

bool Read(const std::string& path, std::string& data, bool& exists) {
    const int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        exists = false;
        if (errno != ENOENT) return false;
        data.clear();
        return true;
    }
    exists = true;
    std::string result;
    bool ok = true;
    char buffer[4096];
    for (;;) {
        const auto count = read(fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { ok = false; break; }
        if (count == 0) break;
        if (result.size() + static_cast<size_t>(count) > MaxConfigSize) { ok = false; break; }
        result.append(buffer, static_cast<size_t>(count));
    }
    if (close(fd) != 0) ok = false;
    if (ok) data = std::move(result);
    return ok;
}

bool WriteAndClose(int fd, const std::string& data) {
    bool ok = true;
    size_t offset = 0;
    while (offset < data.size()) {
        const auto count = write(fd, data.data() + offset, data.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = false; break; }
        offset += static_cast<size_t>(count);
    }
    if (ok && fsync(fd) != 0) ok = false;
    if (close(fd) != 0) ok = false;
    return ok;
}

bool Verify(const std::string& path, const std::string& expected) {
    std::string actual;
    bool exists = false;
    return Read(path, actual, exists) && exists && actual == expected;
}

bool EnsureParent(const std::string& path) {
    for (size_t i = 1; i < path.size(); ++i) if (path[i] == '/') {
        const auto directory = path.substr(0, i);
        if (mkdir(directory.c_str(), 0777) == 0) continue;
        struct stat info{};
        if (errno != EEXIST || stat(directory.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) return false;
    }
    return true;
}
} // namespace

ConfigResult ConfigStore::Load(State& state) {
    loaded_ = false;
    std::string data;
    bool exists = false;
    if (!Read(path_, data, exists)) return {ConfigError::Read, recoveryRequired_, recoveryBackup_};
    if (recoveryRequired_ && (exists != exists_ || data != original_))
        return {ConfigError::Write, true, recoveryBackup_};
    State parsed;
    if (!DecodeConfig(data, parsed)) return {ConfigError::Invalid, false, {}};
    original_ = std::move(data);
    baseline_ = parsed;
    exists_ = exists;
    loaded_ = true;
    recoveryRequired_ = false;
    recoveryBackup_.clear();
    state = parsed;
    return {};
}

bool ConfigStore::Dirty(const State& state) const {
    std::string patched;
    return loaded_ && (!PatchConfig(original_, baseline_, state, patched) || patched != original_);
}

ConfigResult ConfigStore::Save(const State& state) {
    if (!loaded_ || !ValidPreferences(state)) return {ConfigError::Invalid, false, {}};
    std::string patched;
    if (!PatchConfig(original_, baseline_, state, patched) || patched.size() > MaxConfigSize)
        return {ConfigError::Invalid, false, {}};
    if (patched == original_) return {};
    auto unchanged = [&]() {
        std::string current;
        bool exists = false;
        return Read(path_, current, exists) && exists == exists_ && current == original_;
    };
    if (!unchanged()) return {ConfigError::Conflict, false, {}};

    std::string backup;
    if (exists_) {
        int fd = -1;
        for (int i = 0; i < 1000; ++i) {
            backup = path_ + ".borealis-backup." + std::to_string(i);
            fd = open(backup.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
            if (fd >= 0 || errno != EEXIST) break;
        }
        if (fd < 0) return {ConfigError::Backup, false, {}};
        if (!WriteAndClose(fd, original_) || !Verify(backup, original_))
            return {ConfigError::Backup, false, {}};
        // Retain each verified backup, including after a failed/conflicting write.
        if (!unchanged()) return {ConfigError::Conflict, false, backup};
    } else if (!EnsureParent(path_)) return {ConfigError::Write, false, {}};

    const int fd = open(path_.c_str(), O_WRONLY | (exists_ ? O_TRUNC : O_CREAT | O_EXCL), 0600);
    if (fd < 0) return {ConfigError::Write, false, backup};
    // Do not let a retry back up an incomplete result as if it were known good.
    loaded_ = false;
    recoveryRequired_ = true;
    recoveryBackup_ = backup;
    if (!WriteAndClose(fd, patched)) return {ConfigError::Write, true, backup};
    if (!Verify(path_, patched)) return {ConfigError::Verify, true, backup};
    original_ = std::move(patched);
    baseline_ = state;
    loaded_ = exists_ = true;
    recoveryRequired_ = false;
    recoveryBackup_.clear();
    return {ConfigError::None, false, backup};
}
} // namespace TextureSettings
