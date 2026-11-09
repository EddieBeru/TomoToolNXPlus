#include "texture_settings_config.h"
#include "texture_settings_store.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <map>
#include <string>
#include <unistd.h>

using namespace TextureSettings;
using namespace TextureProcessor;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (false)

enum class Failure { None, Read, ReadClose, BackupOpen, BackupWrite, BackupFlush,
    BackupClose, BackupVerify, TargetOpen, TargetWrite, TargetFlush, TargetClose, TargetVerify, ShortWrites };
static Failure failure;
struct File { bool backup, writing; };
static std::map<int, File> files;
static std::string target;
static bool wroteTarget;
extern "C" int __real_open(const char*, int, ...);
extern "C" ssize_t __real_read(int, void*, size_t);
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" int __real_close(int);
extern "C" int __real_fsync(int);
extern "C" int __wrap_open(const char* path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) { va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args); }
    const bool backup = std::string(path).find(".borealis-backup.") != std::string::npos;
    const bool writing = flags & O_WRONLY;
    if ((backup && writing && failure == Failure::BackupOpen) ||
        (!backup && writing && failure == Failure::TargetOpen)) { errno = EIO; return -1; }
    int fd = __real_open(path, flags, mode);
    if (fd >= 0) files[fd] = {backup, writing};
    if (fd >= 0 && !backup && writing) wroteTarget = true;
    return fd;
}
extern "C" ssize_t __wrap_read(int fd, void* data, size_t count) {
    const auto file = files.at(fd);
    if (failure == Failure::Read || (file.backup && failure == Failure::BackupVerify) ||
        (!file.backup && wroteTarget && failure == Failure::TargetVerify)) { errno = EIO; return -1; }
    return __real_read(fd, data, count);
}
extern "C" ssize_t __wrap_write(int fd, const void* data, size_t count) {
    const auto file = files.at(fd);
    if ((file.backup && failure == Failure::BackupWrite) ||
        (!file.backup && failure == Failure::TargetWrite)) { errno = EIO; return -1; }
    return __real_write(fd, data, failure == Failure::ShortWrites ? std::min(count, size_t(7)) : count);
}
extern "C" int __wrap_fsync(int fd) {
    const auto file = files.at(fd);
    if ((file.backup && failure == Failure::BackupFlush) ||
        (!file.backup && failure == Failure::TargetFlush)) { errno = EIO; return -1; }
    return __real_fsync(fd);
}
extern "C" int __wrap_close(int fd) {
    const auto file = files.at(fd); files.erase(fd);
    const int result = __real_close(fd);
    if ((!file.writing && failure == Failure::ReadClose) ||
        (file.backup && file.writing && failure == Failure::BackupClose) ||
        (!file.backup && file.writing && failure == Failure::TargetClose)) { errno = EIO; return -1; }
    return result;
}

static void Put(const std::string& text) {
    FILE* f = std::fopen(target.c_str(), "wb"); CHECK(f);
    CHECK(std::fwrite(text.data(), 1, text.size(), f) == text.size()); CHECK(std::fclose(f) == 0);
}
static std::string Get(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb"); CHECK(f);
    std::string text; char buffer[1024]; size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), f))) text.append(buffer, n);
    CHECK(!std::ferror(f)); CHECK(std::fclose(f) == 0); return text;
}

int main() {
    char dir[] = "/tmp/tomotool-config-test-XXXXXX"; CHECK(mkdtemp(dir));
    target = std::string(dir) + "/config.ini";
    const std::string original = "# synthetic\r\ntheme=old-theme\r\naccent_color=010203\r\n"
        "bg_color=040506\r\nunknown=future value\r\nencoder=custom\r\nbc1_mode=3color\r\n"
        "fit_mode=contain\r\ncanvas_shape=book\r\nmatte=a1B2c3\r\nexport_path=/synthetic/exports";
    State state;
    CHECK(DecodeConfig(original, state)); CHECK(ValidPreferences(state));
    CHECK(ValueIndex(state, Row::Background) == -1 && state.matte.g == 0xb2);
    std::string patched; CHECK(PatchConfig(original, state, state, patched)); CHECK(patched == original);
    State changed = state; CHECK(SelectValue(changed, Row::Encoder, 1));
    CHECK(!IsEnabled(changed, Row::Bc1Mode)); CHECK(changed.bc1Mode == Bc1Mode::ThreeColor);
    CHECK(!SelectValue(changed, Row::Bc1Mode, 0));
    CHECK(PatchConfig(original, state, changed, patched));
    auto expected = original; expected.replace(expected.find("encoder=custom"), 14, "encoder=pca"); CHECK(patched == expected);
    for (auto bad : {"encoder=unknown", "matte=xx2233", "fit_mode=invalid", "canvas_shape=", "bc1_mode=5color"}) {
        State before = state; CHECK(!DecodeConfig(bad, state)); CHECK(state.matte.r == before.matte.r);
    }
    CHECK(!DecodeConfig(std::string("x=\0a", 4), state));
    CHECK(DecodeConfig("matte=123456\nfit_mode=cover\n", state));
    CHECK(!IsEnabled(state, Row::Background) && state.matte.r == 0x12);
    CHECK(DecodeConfig("encoder=pca\nencoder=custom\n", state));
    changed = state; CHECK(SelectValue(changed, Row::Encoder, 1));
    CHECK(PatchConfig("encoder=pca\nencoder=custom\n", state, changed, patched));
    CHECK(patched == "encoder=pca\nencoder=pca\n");
    changed.encoder = static_cast<Bc1Encoder>(99); CHECK(!ValidPreferences(changed));
    CHECK(!PatchConfig(original, state, changed, patched));

    int backupCount = 0;
    for (auto fault : {Failure::None, Failure::ShortWrites, Failure::Read, Failure::ReadClose,
            Failure::BackupOpen, Failure::BackupWrite, Failure::BackupFlush, Failure::BackupClose,
            Failure::BackupVerify, Failure::TargetOpen, Failure::TargetWrite, Failure::TargetFlush,
            Failure::TargetClose, Failure::TargetVerify}) {
        failure = Failure::None; wroteTarget = false; Put(original);
        ConfigStore store(target); State prefs; CHECK(store.Load(prefs));
        CHECK(!store.Dirty(prefs)); CHECK(SelectValue(prefs, Row::Encoder, 1)); CHECK(store.Dirty(prefs));
        failure = fault;
        const auto result = store.Save(prefs);
        const bool success = fault == Failure::None || fault == Failure::ShortWrites;
        CHECK(static_cast<bool>(result) == success);
        if (success || result.mayHaveChanged) {
            CHECK(!result.backupPath.empty()); CHECK(Get(result.backupPath) == original);
            if (success) { CHECK(Get(target) == expected); CHECK(!store.Dirty(prefs)); }
            else {
                CHECK(!store.Save(prefs));
                failure = Failure::None;
                if (Get(target) != original) CHECK(!store.Load(state));
                Put(original);
                CHECK(store.Load(state));
            }
        } else CHECK(Get(target) == original);
        CHECK(files.empty());
        ++backupCount;
    }
    failure = Failure::None; wroteTarget = false;
    Put(original); ConfigStore conflict(target); CHECK(conflict.Load(state));
    changed = state; CHECK(SelectValue(changed, Row::Encoder, 1));
    Put(original + "\nexternal=changed\n");
    CHECK(conflict.Save(changed).error == ConfigError::Conflict);
    CHECK(Get(target) == original + "\nexternal=changed\n");
    Put("encoder=future\n"); ConfigStore invalid(target); CHECK(!invalid.Load(state)); CHECK(!invalid.Save(changed));
    CHECK(Get(target) == "encoder=future\n");
    Put(std::string(65537, 'x')); ConfigStore oversized(target); CHECK(!oversized.Load(state));
    CHECK(unlink(target.c_str()) == 0);
    ConfigStore fresh(target); CHECK(fresh.Load(state)); CHECK(!fresh.Exists());
    CHECK(SelectValue(state, Row::Shape, 2)); CHECK(fresh.Save(state));
    CHECK(Get(target) == "canvas_shape=tv\n");
    ConfigStore reopened(target); CHECK(reopened.Load(changed)); CHECK(changed.canvasShape == CanvasShape::Tv);
    CHECK(unlink(target.c_str()) == 0);
    for (int i = 0; i < backupCount; ++i) unlink((target + ".borealis-backup." + std::to_string(i)).c_str());
    CHECK(rmdir(dir) == 0);
    std::puts("Texture config preservation, persistence and failure checks passed.");
}
