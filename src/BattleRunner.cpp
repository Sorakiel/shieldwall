#include "BattleRunner.h"
#include <string>

RunResult BattleRunner::run(BattleEngine& engine, Army& a, Army& b, std::mt19937& logRng,
                            BattleMode mode, int turn, const std::function<void(int)>& onTurn) {
    ui_.setBattleProgress(turn, mode);
    ui_.showArmies(a, b);
    while (!engine.finished()) {
        const auto action = ui_.waitForTurn(mode);
        if (action == TurnAction::Exit) return {false, turn, mode};
        if (action == TurnAction::Automatic || action == TurnAction::Manual) {
            mode = action == TurnAction::Automatic ? BattleMode::Automatic : BattleMode::Manual;
            ui_.setBattleProgress(turn, mode);
            continue;
        }
        const auto events = engine.nextTurn();
        ++turn;
        // Сохраняем завершённый ход до отрисовки: закрытие окна не теряет прогресс.
        if (onTurn) onTurn(turn);
        ui_.showEvents(events, logRng);
        ui_.message("[Ход " + std::to_string(turn) + "] Уборка завершена. Состояние строёв:");
        ui_.setBattleProgress(turn, mode);
        ui_.showArmies(a, b);
    }
    ui_.showResult(engine.winner());
    return {true, turn, mode};
}
