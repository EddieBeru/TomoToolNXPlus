#pragma once

#include <functional>
#include <string>

namespace SaveWorkflow {

enum class File { Mii, Player, Map };

struct Pending {
    bool mii = false;
    bool player = false;
    bool map = false;
    bool external = false;
    bool Any() const { return mii || player || map || external; }
};

struct Operations {
    std::function<std::string()> checkBackup;
    std::function<std::string(File)> validate;
    std::function<std::string(File)> write;
    std::function<std::string()> commit;
};

struct Result {
    std::string error;
    bool writesAttempted = false;
};

// The caller retains edits until the entire sequence succeeds. No rollback.
Result Run(const Pending& pending, const Operations& operations);

} // namespace SaveWorkflow
