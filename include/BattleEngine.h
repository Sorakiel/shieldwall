#pragma once
#include <cstdint>
#include <random>
#include <vector>
#include "Army.h"
#include "BuffGiver.h"
#include "Event.h"

class BattleEngine {
public:
    // ВРЕМЕННО: ограничиваем бой 1000 полными ходами, чтобы лечение и
    // клонирование не зацикливали автобой. Заменить правилом определения ничьей.
    static constexpr int MaxTurns = 1000;

    BattleEngine(Army& a, Army& b, std::uint32_t seed);
    std::vector<BattleEvent> nextTurn();   // один полный ход, три фазы
    std::vector<BattleEvent> runToEnd();   // автобой: nextTurn в цикле
    bool finished() const;
    int  winner()   const;                 // 0/1 — победитель, -1 — бой идёт, -2 — ничья

private:
    Army& a_;
    Army& b_;
    std::mt19937 rng_;
    BuffGiver buffsA_;
    BuffGiver buffsB_;
    int turn_ = 0;
};
