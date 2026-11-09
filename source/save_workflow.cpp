#include "save_workflow.h"

namespace SaveWorkflow {
namespace {

const char* Filename(File file) {
    switch (file) {
        case File::Mii: return "Mii.sav";
        case File::Player: return "Player.sav";
        case File::Map: return "Map.sav";
    }
    return "save";
}

} // namespace

Result Run(const Pending& pending, const Operations& operations) {
    Result result;
    if (!pending.Any()) return result;
    if (!operations.checkBackup || !operations.validate || !operations.write || !operations.commit) {
        result.error = "Incomplete save operations";
        return result;
    }
    result.error = operations.checkBackup();
    if (!result.error.empty()) return result;

    const struct { File file; bool dirty; } files[] = {
        {File::Mii, pending.mii}, {File::Player, pending.player}, {File::Map, pending.map},
    };
    for (const auto& item : files) {
        if (!item.dirty) continue;
        result.error = operations.validate(item.file);
        if (!result.error.empty()) {
            result.error = std::string(Filename(item.file)) + ": " + result.error;
            return result;
        }
    }
    for (const auto& item : files) {
        if (!item.dirty) continue;
        result.writesAttempted = true;
        result.error = operations.write(item.file);
        if (!result.error.empty()) {
            result.error = std::string(Filename(item.file)) + ": " + result.error;
            return result;
        }
        result.error = operations.commit();
        if (!result.error.empty()) {
            result.error = std::string(Filename(item.file)) + " commit: " + result.error;
            return result;
        }
    }
    if (pending.external) {
        result.writesAttempted = true;
        result.error = operations.commit();
    }
    return result;
}

} // namespace SaveWorkflow
