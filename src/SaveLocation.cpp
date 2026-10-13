#include "SaveLocation.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <cstdint>
#include <sstream>

namespace fs = std::filesystem;

namespace {
bool sameContents(const fs::path& a, const fs::path& b) {
    std::error_code error;
    const auto aSize = fs::file_size(a, error);
    if (error) return false;
    const auto bSize = fs::file_size(b, error);
    if (error || aSize != bSize) return false;
    std::ifstream first(a, std::ios::binary), second(b, std::ios::binary);
    if (!first || !second) return false;
    std::array<char, 4096> left{}, right{};
    while (first && second) {
        first.read(left.data(), static_cast<std::streamsize>(left.size()));
        second.read(right.data(), static_cast<std::streamsize>(right.size()));
        if (first.gcount() != second.gcount() ||
            !std::equal(left.begin(), left.begin() + first.gcount(), right.begin())) return false;
    }
    return first.eof() && second.eof();
}

void importDirectory(const fs::path& source, SaveLocation& location) {
    std::error_code error;
    if (!fs::is_directory(source, error) || fs::equivalent(source, location.directory, error)) return;
    const auto sourceKey = fs::weakly_canonical(source, error).u8string();
    if (error) return;
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char byte : sourceKey) { hash ^= byte; hash *= 1099511628211ull; }
    std::ostringstream key;
    key << std::hex << hash;
    const auto marker = location.directory / (".imported-" + key.str());
    if (fs::exists(marker, error)) return;
    bool hadFiles = false, successful = true;
    fs::directory_iterator files(source, error);
    if (error) {
        location.messages.push_back("Не удалось прочитать старые сохранения: " + source.u8string());
        return;
    }
    for (; files != fs::directory_iterator{}; files.increment(error)) {
        if (error) break;
        const auto& entry = *files;
        if (!entry.is_regular_file(error) || entry.path().extension() != ".txt") continue;
        hadFiles = true;
        auto destination = location.directory / entry.path().filename();
        if (fs::exists(destination, error)) {
            if (sameContents(entry.path(), destination)) continue;
            // Conflicts are retained separately; matching copies are not imported again.
            bool duplicate = false;
            fs::directory_iterator existing(location.directory, error);
            for (; !error && existing != fs::directory_iterator{}; existing.increment(error)) {
                if (existing->is_regular_file(error) && existing->path().extension() == ".txt" &&
                    sameContents(entry.path(), existing->path())) { duplicate = true; break; }
            }
            if (duplicate) continue;
            for (int number = 1;; ++number) {
                destination = location.directory / ("legacy-" + std::to_string(number) + ".txt");
                if (!fs::exists(destination, error)) break;
                if (error) break;
            }
        }
        error.clear();
        if (fs::copy_file(entry.path(), destination, fs::copy_options::none, error)) {
            std::error_code ignored;
            const auto time = entry.last_write_time(ignored);
            if (!ignored) fs::last_write_time(destination, time, ignored);
            location.messages.push_back("Найдено старое сохранение: " +
                entry.path().filename().u8string() + " → " + destination.filename().u8string());
        } else if (error) {
            successful = false;
            location.messages.push_back("Не удалось перенести сохранение: " +
                entry.path().filename().u8string() + ". " + error.message());
        }
    }
    if (error) {
        successful = false;
        location.messages.push_back("Ошибка чтения папки сохранений: " + source.u8string());
    }
    // A completed migration must not resurrect files deleted through the new menu.
    if (hadFiles && successful) {
        std::ofstream stamp(marker);
        if (!stamp) location.messages.push_back("Не удалось отметить перенос старых сохранений.");
    }
}
}  // namespace

SaveLocation prepareSaveLocation(const fs::path& executableDirectory, const fs::path& workingDirectory) {
    SaveLocation result;
    result.directory = fs::absolute(executableDirectory) / "saves";
    std::error_code error;
    fs::create_directories(result.directory, error);
    if (error) {
        result.messages.push_back("Не удалось создать папку сохранений: " + result.directory.u8string());
        return result;
    }
    importDirectory(fs::absolute(workingDirectory) / "saves", result);
    // Older builds run from a shell typically wrote to the source repository.
    for (auto parent = fs::absolute(executableDirectory); !parent.empty();) {
        if (fs::is_regular_file(parent / "CMakeLists.txt", error) &&
            fs::is_regular_file(parent / "data/units.txt", error)) {
            importDirectory(parent / "saves", result);
            break;
        }
        const auto next = parent.parent_path();
        if (next == parent) break;
        parent = next;
    }
    return result;
}
