#pragma once
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include "SaveService.h"

// Строка списка сохранений для экрана "Загрузить". Битый файл не прячется и не
// роняет список: он приходит с valid = false и причиной в error.
struct SaveSummary {
    std::string name;       // имя файла без расширения, например "autosave" или "save-2"
    std::string path;
    bool valid = false;
    std::string error;      // понятный текст ошибки, если valid == false
    SavePhase phase = SavePhase::Battle;
    int costLimit = 0;
    std::uint32_t seed = 0;
    int turn = 0;
    std::time_t modified = 0;
};

// Именованные сохранения в одной папке. Формат файла знает SaveService, здесь только
// файлы: список, безопасные имена, свободное имя. Экран и ввод остаются за интерфейсом.
class SaveSlots {
public:
    static constexpr const char* AutosaveName = "autosave";

    // Файлы *.txt из папки: автосохранение первым, остальные от новых к старым.
    // Папки нет - пустой список, это обычное состояние до первого сохранения.
    static std::vector<SaveSummary> list(const std::string& dir);

    // Путь для сохранения под именем игрока. DataError, если имя нельзя использовать:
    // пустое, с путями и спецсимволами, слишком длинное, зарезервированное системой
    // или равное имени автосохранения.
    static std::string pathFor(const std::string& dir, const std::string& name);

    // Путь автосохранения - тот самый файл, что перезаписывается каждый ход.
    static std::string autosavePath(const std::string& dir);

    // Первое свободное имя вида "save-1", "save-2": удобное значение по умолчанию.
    static std::string suggestName(const std::string& dir);
};
