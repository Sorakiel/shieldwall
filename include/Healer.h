#pragma once
#include <cstddef>
#include <random>
#include <string>
#include "Army.h"
#include "Unit.h"

// Целитель. Сила - сколько hp возвращает за действие, дальность - радиус в местах строя.
// Мага не лечит (маг не IHealable). Сам лечится другими целителями.
class Healer : public Unit, public IHealable {
public:
    Healer(std::string name, int maxHp, int melee, int power, int range, int defense, int cost);
    std::unique_ptr<Unit> clone() const override;
    UnitKind kind() const override { return UnitKind::Healer; }
    int meleeAttack() const override { return melee_; }
    int heal(int amount) override { return restore(amount); }
    int power() const { return power_; }
    int range() const { return range_; }

    struct HealResult {
        bool performed = false;   // нашлась раненая цель и целитель имел право действовать
        int restored = 0;
        std::string target;
    };

    // Особое действие. Целитель в первом ряду не лечит. Цель - случайный живой раненый
    // IHealable в радиусе, кроме самого целителя.
    HealResult cast(Army& own, std::size_t position, std::mt19937& rng) const;

private:
    int melee_ = 0, power_ = 0, range_ = 0;
};
