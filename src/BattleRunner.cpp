#include "BattleRunner.h"
#include <string>

void BattleRunner::run(BattleEngine& engine, Army& a, Army& b, std::mt19937& logRng) {
    int turn = 0;
    while (!engine.finished()) {
        const auto events = engine.nextTurn();
        ++turn;
        ui_.showEvents(events, logRng);
        // nextTurn возвращает управление только после уборки обеих армий.
        ui_.message("[Ход " + std::to_string(turn) + "] Уборка завершена. Состояние строёв:");
        ui_.showArmies(a, b);
    }
    ui_.showResult(engine.winner());
}
