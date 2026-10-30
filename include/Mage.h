#pragma once
#include <cstddef>
#include <random>
#include <string>
#include "Army.h"
#include "Unit.h"

// Маг клонирует своих. Сила - шанс успеха в процентах, дальность - радиус в местах строя.
// Сам не лечится и не клонируется: IHealable и IClonable не реализует.
class Mage : public Unit {
public:
    Mage(std::string name, int maxHp, int melee, int power, int range, int defense, int cost);
    std::unique_ptr<Unit> clone() const override;
    UnitKind kind() const override { return UnitKind::Mage; }
    int meleeAttack() const override { return melee_; }
    int power() const { return power_; }
    int range() const { return range_; }

    struct CastResult {
        bool performed = false;   // способность вообще была применена (была цель и право на действие)
        bool success = false;     // бросок на шанс удался и копия появилась
        std::string target;       // кого пытались клонировать
    };

    // Особое действие. position - место мага в own. Маг в первом ряду не колдует.
    // Цель - случайный живой IClonable в радиусе (свой строй, в обе стороны от мага).
    // Копия встаёт перед магом, поэтому маг сдвигается на одно место назад.
    CastResult cast(Army& own, std::size_t position, std::mt19937& rng) const;

private:
    int melee_ = 0, power_ = 0, range_ = 0;
};
