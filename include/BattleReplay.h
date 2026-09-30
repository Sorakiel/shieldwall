#pragma once
#include <memory>
#include "Army.h"
#include "BattleEngine.h"
#include "SaveService.h"

// Восстановленный бой. Армии лежат в куче, потому что BattleEngine хранит на них
// ссылки: при переезде объектов по стеку ссылки бы повисли.
struct BattleSession {
    std::unique_ptr<Army> armyA;
    std::unique_ptr<Army> armyB;
    std::unique_ptr<BattleEngine> engine;
    int turn = 0;
};

class BattleReplay {
public:
    // Строит армии из записей, создаёт движок с сохранённым seed и проигрывает
    // data.turn ходов. Результат совпадает с тем боем, что сохраняли, только если
    // вся случайность боя идёт через единственный mt19937 внутри движка.
    // DataError, если бой в этом месте уже закончился раньше, чем записано в файле.
    static BattleSession replay(const SaveData& data);
};
