#pragma once
#include "Unit.h"
#include "gulyay_gorod.h"

class GulyayGorodAdapter final : public Unit {
public:
    GulyayGorodAdapter(std::string name, int cost)
        : Unit(std::move(name), fort::GulyayGorod::kMaxDurability, 0, cost) {}

    UnitKind kind() const override { return UnitKind::GulyayGorod; }
    int meleeAttack() const override { return 0; }
    int hp() const override { return fort_.durability(); }

    // Плоская защита Unit равна нулю: процент брони применяет само укрепление.
    void takeDamage(int rawPower) override {
        // Даже при максимальной броне этой силы достаточно для полного разрушения.
        fort_.takeHit(std::min(rawPower, 5 * maxHp()));
    }

    std::unique_ptr<Unit> clone() const override {
        auto copy = std::make_unique<GulyayGorodAdapter>(name(), cost());
        // Служебная копия армии сохраняет прочность некопируемого объекта.
        copy->takeDamage(5 * maxHp());
        copy->fort_.repair(hp());
        return copy;
    }

private:
    fort::GulyayGorod fort_;
};
