#pragma once
#include <cstddef>
#include <random>
#include <string>
#include "Army.h"
#include "UnitDecorator.h"

// Особая способность лёгкого пехотинца: стоя рядом с тяжёлым, с некоторым шансом
// даёт ему бонус (шлем, щит, копьё, конь). Не больше одного бонуса за ход.
// Движок вызывает newTurn() в начале хода и tryGrant() для каждого живого лёгкого.
class BuffGiver {
public:
    explicit BuffGiver(int chancePercent = 25) : chance_(chancePercent) {}

    struct Result {
        bool granted = false;
        std::string giver;
        std::string receiver;   // имя до выдачи бонуса
        BuffKind buff = BuffKind::Helmet;
    };

    void newTurn() { usedThisTurn_ = false; }
    bool usedThisTurn() const { return usedThisTurn_; }

    // lightIndex - место лёгкого в строю. Соседи - места lightIndex-1 и lightIndex+1.
    // Бонус получает случайный живой тяжёлый сосед; тяжёлого в обёртке опознаём по kind().
    Result tryGrant(Army& army, std::size_t lightIndex, std::mt19937& rng);

private:
    int chance_;
    bool usedThisTurn_ = false;
};
