#pragma once
#include <functional>
#include <random>
#include "Army.h"
#include "BattleEngine.h"
#include "UserInterface.h"

struct RunResult {
    bool finished;
    int turn;
    BattleMode mode;
};

class BattleRunner {
public:
    explicit BattleRunner(UserInterface& ui) : ui_(ui) {}
    RunResult run(BattleEngine& engine, Army& a, Army& b, std::mt19937& logRng,
                  BattleMode mode = BattleMode::Automatic, int turn = 0,
                  const std::function<void(int)>& onTurn = {},
                  const std::function<void(int)>& onSave = {});

private:
    UserInterface& ui_;
};
