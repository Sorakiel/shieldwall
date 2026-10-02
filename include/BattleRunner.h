#pragma once
#include <random>
#include "Army.h"
#include "BattleEngine.h"
#include "ConsoleUI.h"

class BattleRunner {
public:
    explicit BattleRunner(ConsoleUI& ui) : ui_(ui) {}
    void run(BattleEngine& engine, Army& a, Army& b, std::mt19937& logRng);

private:
    ConsoleUI& ui_;
};
