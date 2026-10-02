#pragma once
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>
#include "Army.h"
#include "UnitCatalog.h"

// Юнит в том виде, в каком он вошёл в бой: имя плюс характеристики.
struct UnitRecord {
    UnitSpec spec;
    std::string name;
};

// Всё, что нужно для восстановления боя. Снимков hp нет намеренно: бой
// проигрывается заново с тем же seed, поэтому файл мал, а результат совпадает.
struct SaveData {
    std::uint32_t seed = 0;   // seed движка боя
    int turn = 0;             // сколько ходов уже сыграно
    std::vector<UnitRecord> armyA;
    std::vector<UnitRecord> armyB;
};

class SaveService {
public:
    // Состав армии в строю. Снимать нужно до первого хода: записывается maxHp,
    // а не текущее hp, потому что бой восстанавливается с самого начала.
    static std::vector<UnitRecord> snapshot(Army& army);

    static void write(const SaveData& data, std::ostream& out);
    static void saveToFile(const SaveData& data, const std::string& path);

    static SaveData read(std::istream& in, const std::string& source);
    static SaveData loadFromFile(const std::string& path);

    static constexpr int SupportedVersion = 1;
};
