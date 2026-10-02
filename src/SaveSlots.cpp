#include "SaveSlots.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

const std::size_t MaxNameBytes = 64;

// Имена, которые Windows не даёт использовать для файлов, даже с расширением
bool isReservedOnWindows(const std::string& name) {
    std::string upper;
    for (char c : name) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL") return true;
    return upper.size() == 4 && (upper.compare(0, 3, "COM") == 0 || upper.compare(0, 3, "LPT") == 0) &&
           upper[3] >= '1' && upper[3] <= '9';
}

std::time_t toTimeT(fs::file_time_type time) {
    // В C++17 нет прямого перевода file_time_type в system_clock, поэтому через разницу с "сейчас"
    auto shifted = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        time - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(shifted);
}

}  // namespace

std::string SaveSlots::autosavePath(const std::string& dir) {
    return (fs::path(dir) / (std::string(AutosaveName) + ".txt")).string();
}

std::string SaveSlots::pathFor(const std::string& dir, const std::string& name) {
    if (name.empty()) {
        throw DataError("введите имя сохранения");
    }
    if (name.size() > MaxNameBytes) {
        throw DataError("имя сохранения слишком длинное");
    }
    if (name != dataformat::trim(name) || name.front() == '.' || name.back() == '.') {
        throw DataError("имя не должно начинаться или заканчиваться пробелом или точкой");
    }
    for (unsigned char c : name) {
        // Всё от 0x80 - буквы не из ASCII (кириллица) в UTF-8, их разрешаем
        bool allowed = c >= 0x80 || std::isalnum(c) || c == '-' || c == '_' || c == ' ';
        if (!allowed) {
            throw DataError("в имени можно использовать буквы, цифры, пробел, «-» и «_»");
        }
    }
    if (isReservedOnWindows(name)) {
        throw DataError("имя «" + name + "» зарезервировано системой");
    }
    if (name == AutosaveName) {
        throw DataError("имя «autosave» занято автосохранением");
    }
    return (fs::path(dir) / (name + ".txt")).string();
}

std::string SaveSlots::suggestName(const std::string& dir) {
    for (int n = 1;; ++n) {
        std::string name = "save-" + std::to_string(n);
        std::error_code ignored;
        if (!fs::exists(pathFor(dir, name), ignored)) {
            return name;
        }
    }
}

std::vector<SaveSummary> SaveSlots::list(const std::string& dir) {
    std::vector<SaveSummary> result;
    std::error_code error;
    fs::directory_iterator it(dir, error);
    if (error) {
        return result;
    }
    for (const fs::directory_entry& entry : it) {
        if (!entry.is_regular_file(error) || entry.path().extension() != ".txt") {
            continue;
        }
        SaveSummary item;
        item.name = entry.path().stem().string();
        item.path = entry.path().string();
        auto time = entry.last_write_time(error);
        item.modified = error ? 0 : toTimeT(time);
        try {
            SaveData data = SaveService::loadFromFile(item.path);
            item.valid = true;
            item.phase = data.phase;
            item.costLimit = data.costLimit;
            item.seed = data.seed;
            item.turn = data.turn;
        } catch (const DataError& e) {
            item.error = e.what();
        }
        result.push_back(std::move(item));
    }
    std::sort(result.begin(), result.end(), [](const SaveSummary& a, const SaveSummary& b) {
        bool aAuto = a.name == AutosaveName;
        bool bAuto = b.name == AutosaveName;
        if (aAuto != bAuto) return aAuto;
        if (a.modified != b.modified) return a.modified > b.modified;
        return a.name < b.name;
    });
    return result;
}
