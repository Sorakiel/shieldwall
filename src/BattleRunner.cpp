#include "BattleRunner.h"
#include <string>

RunResult BattleRunner::run(BattleEngine& engine, Army& a, Army& b, std::mt19937& logRng, BattleMode mode,
                            int turn, const std::function<void(int)>& onTurn,
                            const std::function<void(int)>& onSave) {
    ui_.setBattleProgress(turn, mode);
    ui_.showArmies(a, b);
    while (!engine.finished()) {
        const auto action = ui_.waitForTurn(mode);
        if (action == TurnAction::Exit)
            return {false, turn, mode};
        if (action == TurnAction::Automatic || action == TurnAction::Manual) {
            mode = action == TurnAction::Automatic ? BattleMode::Automatic : BattleMode::Manual;
            ui_.setBattleProgress(turn, mode);
            continue;
        }
        if (action == TurnAction::Save) {
            if (onSave)
                onSave(turn);
            continue;
        }
        const auto events = engine.nextTurn();
        // A calculated turn becomes a checkpoint only after its playback finishes.
        // During playback saves use the previous fully presented turn.
        struct HandlerScope {
            UserInterface& ui;
            ~HandlerScope() {
                ui.setPlaybackHandler({});
            }
        } handlerScope{ui_};
        ui_.setPlaybackHandler([&](TurnAction request) {
            if (request == TurnAction::Exit)
                return false;
            if (request == TurnAction::Save) {
                if (onSave)
                    onSave(turn);
            } else if (request == TurnAction::Automatic || request == TurnAction::Manual) {
                mode = request == TurnAction::Automatic ? BattleMode::Automatic : BattleMode::Manual;
                ui_.setBattleProgress(turn + 1, mode);
            }
            return true;
        });
        try {
            ui_.showEvents(events, logRng);
        } catch (const PlaybackStopped&) {
            return {false, turn, mode};
        }
        ui_.setPlaybackHandler({});
        ++turn;
        if (onTurn)
            onTurn(turn);
        ui_.message("Уборка завершена. Состояние строёв:");
        ui_.setBattleProgress(turn, mode);
        ui_.showArmies(a, b);
    }
    ui_.showResult(engine.winner());
    return {true, turn, mode};
}
