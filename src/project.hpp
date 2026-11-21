#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace moon {

struct EntryResolution {
    std::filesystem::path project_root;
    std::filesystem::path entry_file;
    std::vector<std::filesystem::path> candidates;
    std::string error;

    [[nodiscard]] bool ok() const { return error.empty(); }
};

// Resolves main.mncyt first, then a unique init*.mncyt candidate in the root
// or its immediate src/ directory. Discovery is intentionally non-recursive.
EntryResolution resolve_entry(const std::filesystem::path& project_path);

} // namespace moon
