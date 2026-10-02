#include "BattleReplay.h"
#include "UnitFactory.h"

BattleSession BattleReplay::replay(const SaveData& data) {
    if (data.phase == SavePhase::Recruitment) {
        throw DataError("в фазе закупки боя ещё нет, восстанавливать нечего");
    }
    auto build = [](const std::vector<UnitRecord>& records) {
        auto army = std::make_unique<Army>();
        for (const UnitRecord& r : records) {
            army->add(UnitFactory::create(r.spec, r.name));
        }
        return army;
    };

    BattleSession session;
    session.armyA = build(data.armyA);
    session.armyB = build(data.armyB);
    session.engine = std::make_unique<BattleEngine>(*session.armyA, *session.armyB, data.seed);

    for (int t = 0; t < data.turn; ++t) {
        if (session.engine->finished()) {
            throw DataError("сохранение повреждено: бой закончился на ходу " + std::to_string(t) +
                            ", а в файле записан ход " + std::to_string(data.turn));
        }
        session.engine->nextTurn();
    }
    session.turn = data.turn;
    return session;
}
