#pragma once
#include <cstdint>
#include <random>
#include <vector>
#include "Army.h"
#include "Event.h"

class BattleEngine {
public:
    BattleEngine(Army& a, Army& b, std::uint32_t seed);
    std::vector<BattleEvent> nextTurn();   // один полный ход, три фазы
    std::vector<BattleEvent> runToEnd();   // автобой: nextTurn в цикле
    bool finished() const;
    int  winner()   const;                 // -1 пока бой идёт

private:
    Army& a_;
    Army& b_;
    std::mt19937 rng_;
    int turn_ = 0;
};
