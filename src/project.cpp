#include "project.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace moon {
namespace {

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool is_source_file(const std::filesystem::path& path) {
    return path.extension() == ".monc";
}

void collect_init_files(const std::filesystem::path& directory,
                        std::vector<std::filesystem::path>& result) {
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec) || ec) return;

    for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
        const auto& path = it->path();
        std::error_code type_error;
        if (it->is_regular_file(type_error) && !type_error && is_source_file(path) &&
            lowercase(path.stem().string()).find("init") != std::string::npos) {
            result.push_back(path);
        }
    }
}

} // namespace

EntryResolution resolve_entry(const std::filesystem::path& project_path) {
    EntryResolution result;
    std::error_code ec;
    result.project_root = std::filesystem::weakly_canonical(project_path, ec);
    if (ec) result.project_root = std::filesystem::absolute(project_path, ec);
    if (ec || !std::filesystem::is_directory(result.project_root, ec)) {
        result.error = "Project directory does not exist: " + project_path.string();
        return result;
    }

    const auto main_file = result.project_root / "main.monc";
    if (std::filesystem::is_regular_file(main_file, ec) && !ec) {
        result.entry_file = main_file;
        return result;
    }

    collect_init_files(result.project_root, result.candidates);
    collect_init_files(result.project_root / "src", result.candidates);
    std::sort(result.candidates.begin(), result.candidates.end());

    if (result.candidates.size() == 1) {
        result.entry_file = result.candidates.front();
    } else if (result.candidates.empty()) {
        result.error = "No entry point found. Add main.monc or one init*.monc file in the project root or src/.";
    } else {
        result.error = "Ambiguous entry point. Add main.monc or leave exactly one init*.monc candidate:";
    }
    return result;
}

} // namespace moon
