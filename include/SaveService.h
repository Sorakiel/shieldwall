#pragma once
#include <cstdint>
#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>
#include "Army.h"
#include "BattleEngine.h"
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

// Восстановленный бой. Армии лежат в куче, потому что BattleEngine хранит на них
// ссылки: при переезде объектов по стеку ссылки бы повисли.
struct BattleSession {
    std::unique_ptr<Army> armyA;
    std::unique_ptr<Army> armyB;
    std::unique_ptr<BattleEngine> engine;
    int turn = 0;
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

    // Строит армии из записей, создаёт движок с сохранённым seed и проигрывает
    // data.turn ходов. Результат совпадает с тем боем, что сохраняли, только если
    // вся случайность боя идёт через единственный mt19937 внутри движка.
    // DataError, если бой в этом месте уже закончился раньше, чем записано в файле.
    static BattleSession replay(const SaveData& data);

    static constexpr int SupportedVersion = 1;
};
