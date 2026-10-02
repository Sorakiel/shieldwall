#include "Unit.h"

LightUnit::LightUnit(std::string name, int maxHp, int melee, int defense, int cost)
    : Unit(std::move(name), maxHp, defense, cost), melee_(melee) {
}

std::unique_ptr<Unit> LightUnit::clone() const {
    return std::make_unique<LightUnit>(*this);
}

HeavyUnit::HeavyUnit(std::string name, int maxHp, int melee, int defense, int cost)
    : Unit(std::move(name), maxHp, defense, cost), melee_(melee) {
}

std::unique_ptr<Unit> HeavyUnit::clone() const {
    return std::make_unique<HeavyUnit>(*this);
}

Archer::Archer(std::string name, int maxHp, int melee, int ranged, int range,
    int defense, int cost)
    : Unit(std::move(name), maxHp, defense, cost),
    melee_(melee), ranged_(ranged), range_(range) {
}

std::unique_ptr<Unit> Archer::clone() const {
    return std::make_unique<Archer>(*this);
}