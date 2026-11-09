// Synthetic saves only. Fault injection uses GNU ld wrappers; see README.md.
#include "backup_snapshot.h"
#include "save_editor.h"
#include "save_workflow.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sys/stat.h>
#include <vector>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition); \
        std::exit(1); \
    } \
} while (false)

enum class Fault { None, ShortWrite, CorruptWrite, Read };
static Fault fault = Fault::None;
static int failCloseAt = -1;
static int closeCount = 0;
static bool failMkdir = false;

extern "C" size_t __real_fwrite(const void*, size_t, size_t, FILE*);
extern "C" size_t __real_fread(void*, size_t, size_t, FILE*);
extern "C" int __real_fclose(FILE*);
extern "C" int __real_mkdir(const char*, mode_t);

extern "C" size_t __wrap_fwrite(const void* data, size_t size, size_t count, FILE* stream) {
    if (fault == Fault::ShortWrite) {
        fault = Fault::None;
        return __real_fwrite(data, size, count / 2, stream);
    }
    if (fault == Fault::CorruptWrite && size * count > 0) {
        fault = Fault::None;
        const auto* bytes = static_cast<const unsigned char*>(data);
        std::vector<unsigned char> corrupt(bytes, bytes + size * count);
        corrupt[0] ^= 1;
        return __real_fwrite(corrupt.data(), size, count, stream);
    }
    return __real_fwrite(data, size, count, stream);
}

extern "C" size_t __wrap_fread(void* data, size_t size, size_t count, FILE* stream) {
    if (fault == Fault::Read) { fault = Fault::None; return 0; }
    return __real_fread(data, size, count, stream);
}

extern "C" int __wrap_fclose(FILE* stream) {
    const int result = __real_fclose(stream);
    if (closeCount++ == failCloseAt) { errno = EIO; return EOF; }
    return result;
}

extern "C" int __wrap_mkdir(const char* path, mode_t mode) {
    if (failMkdir) { failMkdir = false; errno = EACCES; return -1; }
    return __real_mkdir(path, mode);
}

struct TemporaryDirectory {
    std::string path;
    TemporaryDirectory() {
        char pattern[] = "/tmp/tomotoolnx-save-safety-XXXXXX";
        const char* result = mkdtemp(pattern);
        CHECK(result);
        path = result;
    }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

static std::vector<unsigned char> Read(const std::string& path) {
    FILE* file = fopen(path.c_str(), "rb");
    CHECK(file);
    CHECK(fseek(file, 0, SEEK_END) == 0);
    const long size = ftell(file);
    CHECK(size >= 0 && fseek(file, 0, SEEK_SET) == 0);
    std::vector<unsigned char> bytes(static_cast<size_t>(size));
    CHECK(fread(bytes.data(), 1, bytes.size(), file) == bytes.size());
    CHECK(fclose(file) == 0);
    return bytes;
}

static SaveEditor::SavFile Fixture() {
    using namespace SaveEditor;
    SavFile save;
    save.loaded = true;
    save.version = 17;
    save.entries = {
        {0x11223344, DT_Int, 42, {}},
        {0xdeadbeef, DT_Bin, 0, {3, 0, 0, 0, 1, 128, 255}},
        {0x456789ab, DT_IntArray, 0, {2, 0, 0, 0, 4, 0, 0, 0, 9, 0, 0, 0}},
        {0x87654321, DT_UInt64, 0, {1, 2, 3, 4, 5, 6, 7, 8}},
        {0xabcdefff, DT_Bool64Key, 0, {}},
    };
    return save;
}

static void CheckSerializer() {
    using namespace SaveEditor;
    TemporaryDirectory temp;
    const std::string path = temp.path + "/synthetic.sav";
    const SavFile original = Fixture();
    CHECK(Save(path, original).empty());
    const auto bytes = Read(path);
    SavFile loaded;
    std::string error;
    CHECK(Load(path, loaded, error));
    CHECK(loaded.version == original.version && loaded.entries.size() == original.entries.size());
    for (const auto& expected : original.entries) {
        bool found = false;
        for (const auto& actual : loaded.entries) {
            if (actual.hash != expected.hash) continue;
            CHECK(actual.type == expected.type && actual.inlineRaw == expected.inlineRaw);
            CHECK(actual.payload == expected.payload);
            found = true;
        }
        CHECK(found);
    }
    SetInt(loaded, 0x11223344, 99);
    CHECK(Save(path, loaded).empty());
    SavFile edited;
    CHECK(Load(path, edited, error));
    CHECK(GetInt(edited, 0x11223344) == 99);
    CHECK(edited.version == original.version);
    for (const auto& field : edited.entries) {
        if (field.hash == 0xdeadbeef) CHECK(field.payload == original.entries[1].payload);
    }
    const auto editedBytes = Read(path);
    auto invalid = loaded;
    invalid.loaded = false;
    CHECK(!Save(path, invalid).empty());
    CHECK(Read(path) == editedBytes);
    invalid = loaded;
    invalid.entries[0].type = static_cast<DataType>(DT_COUNT + 1);
    CHECK(!Save(path, invalid).empty());
    CHECK(Read(path) == editedBytes);
    invalid = loaded;
    invalid.entries[1].payload[0] = 255;
    CHECK(!Save(path, invalid).empty());
    CHECK(Read(path) == editedBytes);
    invalid.entries[1].payload = {255, 255, 255, 255};
    invalid.entries[1].type = DT_IntArray;
    CHECK(!Save(path, invalid).empty());
    CHECK(Read(path) == editedBytes);
    invalid.entries[1].type = DT_BinArray;
    invalid.entries[1].payload = {1, 0, 0, 0, 255, 255, 255, 255};
    CHECK(!Save(path, invalid).empty());
    CHECK(Read(path) == editedBytes);
    CHECK(!Save(temp.path + "/missing/file.sav", loaded).empty());
    fault = Fault::ShortWrite;
    CHECK(Save(path, loaded).find("Short write") != std::string::npos);
    closeCount = 0;
    failCloseAt = 0;
    CHECK(Save(path, loaded).find("close") != std::string::npos);
    failCloseAt = -1;
    CHECK(!Save("/dev/full", loaded).empty());
    CHECK(!bytes.empty());
}

static void CheckWorkflow() {
    using namespace SaveWorkflow;
    for (unsigned mask = 0; mask < 16; ++mask) {
        const Pending pending{bool(mask & 1), bool(mask & 2), bool(mask & 4), bool(mask & 8)};
        std::vector<std::string> calls;
        const Operations operations{
            [&]() { calls.push_back("backup"); return std::string(); },
            [&](File f) { calls.push_back("validate" + std::to_string(int(f))); return std::string(); },
            [&](File f) { calls.push_back("write" + std::to_string(int(f))); return std::string(); },
            [&]() { calls.push_back("commit"); return std::string(); },
        };
        CHECK(Run(pending, operations).error.empty());
        std::vector<std::string> expected;
        if (mask) expected.push_back("backup");
        for (int file = 0; file < 3; ++file)
            if (mask & (1u << file)) expected.push_back("validate" + std::to_string(file));
        for (int file = 0; file < 3; ++file) {
            if (!(mask & (1u << file))) continue;
            expected.push_back("write" + std::to_string(file));
            expected.push_back("commit");
        }
        if (pending.external) expected.push_back("commit");
        CHECK(calls == expected);
    }
    const Pending all{true, true, true, true};
    for (int failAt = 0; failAt < 11; ++failAt) {
        int calls = 0;
        auto attempt = [&]() { return calls++ == failAt ? std::string("injected failure") : std::string(); };
        Operations operations{attempt, [&](File) { return attempt(); }, [&](File) { return attempt(); }, attempt};
        const Result failed = Run(all, operations);
        CHECK(!failed.error.empty());
        CHECK(calls == failAt + 1);
        CHECK(failed.writesAttempted == (failAt >= 4));
        CHECK(all.mii && all.player && all.map && all.external);
        calls = 0;
        operations = {[]() { return std::string(); },
                      [](File) { return std::string(); },
                      [&](File) { ++calls; return std::string(); },
                      []() { return std::string(); }};
        CHECK(Run(all, operations).error.empty());
        CHECK(calls == 3);
    }
}

static void CreateSaveDirectory(const std::string& path) {
    CHECK(mkdir(path.c_str(), 0700) == 0);
    for (const char* file : {"Mii.sav", "Player.sav", "Map.sav"})
        CHECK(SaveEditor::Save(path + "/" + file, Fixture()).empty());
}

static void CheckBackup() {
    TemporaryDirectory temp;
    const std::string source = temp.path + "/source";
    CreateSaveDirectory(source);
    const std::string good = temp.path + "/good";
    CHECK(BackupSnapshot::Create(source + "/", good).empty());
    CHECK(BackupSnapshot::Validate(good).empty());
    CHECK(Read(source + "/Mii.sav") == Read(good + "/Mii.sav"));
    const auto original = Read(good + "/Mii.sav");
    CHECK(!BackupSnapshot::Create(source, good).empty());
    CHECK(Read(good + "/Mii.sav") == original);

    int suffix = 0;
    for (Fault injected : {Fault::ShortWrite, Fault::CorruptWrite, Fault::Read}) {
        const std::string destination = temp.path + "/fault" + std::to_string(suffix++);
        fault = injected;
        CHECK(!BackupSnapshot::Create(source, destination).empty());
        fault = Fault::None;
        CHECK(!BackupSnapshot::Validate(destination).empty());
        CHECK(BackupSnapshot::Validate(good).empty());
    }
    for (int close : {0, 2, 13}) {
        const std::string destination = temp.path + "/close" + std::to_string(close);
        closeCount = 0;
        failCloseAt = close;
        CHECK(!BackupSnapshot::Create(source, destination).empty());
        failCloseAt = -1;
        CHECK(!BackupSnapshot::Validate(destination).empty());
    }
    failMkdir = true;
    CHECK(!BackupSnapshot::Create(source, temp.path + "/mkdir").empty());
    CHECK(!BackupSnapshot::Create(temp.path + "/missing", temp.path + "/missing-source").empty());
    CHECK(!BackupSnapshot::Validate(temp.path + "/missing-source").empty());
    CHECK(remove((source + "/Player.sav").c_str()) == 0);
    CHECK(!BackupSnapshot::Create(source, temp.path + "/missing-save").empty());
    CHECK(!BackupSnapshot::Validate(temp.path + "/missing-save").empty());
    CHECK(BackupSnapshot::Validate(good).empty());

    CHECK(mkdir((source + "/Ugc").c_str(), 0700) == 0);
    CHECK(mkdir((source + "/empty").c_str(), 0700) == 0);
    CHECK(SaveEditor::Save(source + "/Player.sav", Fixture()).empty());
    CHECK(SaveEditor::Save(source + "/Ugc/unknown.bin", Fixture()).empty());
    const std::string nested = temp.path + "/nested";
    CHECK(BackupSnapshot::Create(source, nested).empty());
    CHECK(Read(source + "/Ugc/unknown.bin") == Read(nested + "/Ugc/unknown.bin"));
    struct stat st{};
    CHECK(stat((nested + "/empty").c_str(), &st) == 0 && S_ISDIR(st.st_mode));
}

int main() {
    CheckSerializer();
    CheckWorkflow();
    CheckBackup();
    std::puts("Save safety checks passed.");
}
