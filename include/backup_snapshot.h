#pragma once

#include <functional>
#include <string>

namespace BackupSnapshot {

constexpr const char* CompleteMarker = ".tomotool_backup_complete";
constexpr const char* IncompleteMarker = ".tomotool_backup_incomplete";

// Destination must not exist. A failed snapshot is never marked complete.
std::string Create(const std::string& source, const std::string& destination,
                   const std::function<void()>& fileCopied = {});
std::string Validate(const std::string& destination);

} // namespace BackupSnapshot
