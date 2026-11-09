#include "backup_snapshot.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace BackupSnapshot {
namespace {

std::string Join(const std::string& root, const char* name) {
    return root + (!root.empty() && root.back() == '/' ? "" : "/") + name;
}

std::string VerifyFile(const std::string& source, const std::string& destination) {
    FILE* in = fopen(source.c_str(), "rb");
    if (!in) return "Cannot read backup source: " + source;
    FILE* copy = fopen(destination.c_str(), "rb");
    if (!copy) { fclose(in); return "Cannot verify backup: " + destination; }
    char a[32768], b[32768];
    bool equal = true;
    while (true) {
        const size_t na = fread(a, 1, sizeof(a), in);
        const size_t nb = fread(b, 1, sizeof(b), copy);
        if (na != nb || memcmp(a, b, na) != 0) { equal = false; break; }
        if (na == 0) break;
    }
    equal = equal && !ferror(in) && !ferror(copy);
    const int closeIn = fclose(in), closeCopy = fclose(copy);
    if (!equal || closeIn != 0 || closeCopy != 0) return "Backup verification failed: " + destination;
    return "";
}

std::string CopyFile(const std::string& source, const std::string& destination) {
    FILE* in = fopen(source.c_str(), "rb");
    if (!in) return "Cannot read backup source: " + source;
    FILE* out = fopen(destination.c_str(), "wb");
    if (!out) { fclose(in); return "Cannot create backup file: " + destination; }
    char buffer[32768];
    bool written = true;
    while (true) {
        const size_t n = fread(buffer, 1, sizeof(buffer), in);
        if (n == 0) break;
        if (fwrite(buffer, 1, n, out) != n) { written = false; break; }
    }
    written = written && !ferror(in) && !ferror(out);
    const int closeIn = fclose(in), closeOut = fclose(out);
    if (!written || closeIn != 0 || closeOut != 0) return "Backup copy failed: " + destination;
    return VerifyFile(source, destination);
}

std::string CopyTree(const std::string& source, const std::string& destination,
                     const std::function<void()>& fileCopied) {
    DIR* dir = opendir(source.c_str());
    if (!dir) return "Cannot read backup directory: " + source;
    std::string error;
    while (true) {
        errno = 0;
        const dirent* entry = readdir(dir);
        if (!entry) {
            if (errno) error = "Cannot enumerate backup directory: " + source;
            break;
        }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        const std::string from = Join(source, entry->d_name);
        const std::string to = Join(destination, entry->d_name);
        struct stat st{};
        if (stat(from.c_str(), &st) != 0) { error = "Cannot inspect backup source: " + from; break; }
        if (S_ISDIR(st.st_mode)) {
            if (mkdir(to.c_str(), 0777) != 0) { error = "Cannot create backup directory: " + to; break; }
            error = CopyTree(from, to, fileCopied);
        } else if (S_ISREG(st.st_mode)) {
            error = CopyFile(from, to);
            if (error.empty() && fileCopied) fileCopied();
        } else {
            error = "Unsupported backup entry: " + from;
        }
        if (!error.empty()) break;
    }
    if (closedir(dir) != 0 && error.empty()) error = "Cannot close backup directory: " + source;
    return error;
}

std::string CheckSaveFiles(const std::string& destination) {
    for (const char* name : {"Mii.sav", "Player.sav", "Map.sav"}) {
        const std::string path = destination + "/" + name;
        struct stat st{};
        if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0x20)
            return "Backup is missing a save file: " + path;
        FILE* file = fopen(path.c_str(), "rb");
        if (!file) return "Cannot read backup file: " + path;
        unsigned char magic[4]{};
        const bool valid = fread(magic, 1, sizeof(magic), file) == sizeof(magic) &&
                           memcmp(magic, "\x04\x03\x02\x01", 4) == 0;
        const int closeResult = fclose(file);
        if (!valid || closeResult != 0) return "Invalid backup save file: " + path;
    }
    return "";
}

} // namespace

std::string Create(const std::string& source, const std::string& destination,
                   const std::function<void()>& fileCopied) {
    if (mkdir(destination.c_str(), 0777) != 0) return "Cannot create new backup: " + destination;
    const std::string incomplete = Join(destination, IncompleteMarker);
    FILE* pending = fopen(incomplete.c_str(), "wb");
    if (!pending) return "Cannot mark backup in progress: " + destination;
    if (fclose(pending) != 0) return "Cannot close backup marker: " + destination;
    std::string error = CopyTree(source, destination, fileCopied);
    if (error.empty()) error = CheckSaveFiles(destination);
    if (!error.empty()) return error;
    const std::string complete = Join(destination, CompleteMarker);
    FILE* marker = fopen(complete.c_str(), "wb");
    if (!marker) return "Cannot mark backup complete: " + destination;
    const bool marked = fwrite("complete\n", 1, 9, marker) == 9;
    const int closeResult = fclose(marker);
    if (!marked || closeResult != 0) return "Cannot close backup marker: " + destination;
    if (remove(incomplete.c_str()) != 0) return "Cannot finish backup: " + destination;
    return "";
}

std::string Validate(const std::string& destination) {
    struct stat st{};
    if (stat(Join(destination, IncompleteMarker).c_str(), &st) == 0)
        return "Backup is incomplete: " + destination;
    FILE* marker = fopen(Join(destination, CompleteMarker).c_str(), "rb");
    if (!marker)
        return "Backup did not complete: " + destination;
    char content[10]{};
    const bool valid = fread(content, 1, sizeof(content), marker) == 9 &&
                       memcmp(content, "complete\n", 9) == 0 && !ferror(marker);
    const int closeResult = fclose(marker);
    if (!valid || closeResult != 0) return "Invalid backup marker: " + destination;
    return CheckSaveFiles(destination);
}

} // namespace BackupSnapshot
