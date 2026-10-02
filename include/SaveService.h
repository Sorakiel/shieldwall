#pragma once
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>
#include "Army.h"
#include "UnitCatalog.h"

// Фазы партии, в начале которых можно сохраняться. Setup не нужен: до выбора лимита
// сохранять нечего.
enum class SavePhase { Recruitment, Battle, Result };

// Всё, что нужно для продолжения партии с любой фазы. Снимков hp нет намеренно:
// бой проигрывается заново с тем же seed, поэтому файл мал, а результат совпадает.
// В фазе Recruitment армии могут быть недособраны, в Battle и Result это стартовые
// составы, снятые до первого хода.
struct SaveData {
    SavePhase phase = SavePhase::Battle;
    int costLimit = 0;        // лимит цены каждой армии
    std::uint32_t seed = 0;   // seed движка боя
    int turn = 0;             // сколько ходов уже сыграно, в Recruitment всегда 0
    std::vector<UnitRecord> armyA;
    std::vector<UnitRecord> armyB;
};

class SaveService {
public:
    // Состав армии в строю. Снимать нужно до первого хода: записывается maxHp,
    // а не текущее hp, потому что бой восстанавливается с самого начала.
    static std::vector<UnitRecord> snapshot(Army& army);

    static void write(const SaveData& data, std::ostream& out);
    // Создаёт недостающие папки, чтобы автосохранение в saves/ работало с первого раза
    static void saveToFile(const SaveData& data, const std::string& path);

    static SaveData read(std::istream& in, const std::string& source);
    static SaveData loadFromFile(const std::string& path);

    static constexpr int SupportedVersion = 2;
};
