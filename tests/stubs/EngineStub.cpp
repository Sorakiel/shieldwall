// Временная замена BattleEngine.cpp для тестов сохранений, пока ядро не готово.
// Правил боя здесь нет: ход лишь тратит числа из rng_ и бьёт по первому юниту,
// чего достаточно, чтобы проверить, что seed и номер хода восстанавливают одну картину.
#include "BattleEngine.h"

BattleEngine::BattleEngine(Army& a, Army& b, std::uint32_t seed) : a_(a), b_(b), rng_(seed) {}

std::vector<BattleEvent> BattleEngine::nextTurn() {
    std::uint32_t r = rng_();
    Army& target = (r & 1u) ? a_ : b_;
    if (Unit* front = target.front()) {
        front->takeDamage(1 + static_cast<int>((r >> 1) % 9));
    }
    target.removeDead();
    ++turn_;
    return {};
}

std::vector<BattleEvent> BattleEngine::runToEnd() {
    while (!finished()) nextTurn();
    return {};
}

bool BattleEngine::finished() const { return a_.isDefeated() || b_.isDefeated(); }

int BattleEngine::winner() const {
    if (!finished()) return -1;
    return a_.isDefeated() ? 1 : 0;
}
