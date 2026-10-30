#include "Healer.h"
#include <vector>

Healer::Healer(std::string name, int maxHp, int melee, int power, int range, int defense, int cost)
    : Unit(std::move(name), maxHp, defense, cost), melee_(melee), power_(power), range_(range) {}

std::unique_ptr<Unit> Healer::clone() const {
    return std::make_unique<Healer>(*this);
}

Healer::HealResult Healer::cast(Army& own, std::size_t position, std::mt19937& rng) const {
    HealResult result;
    if (position == 0 || position >= own.size()) return result;   // первый ряд не лечит

    const std::size_t from = position > static_cast<std::size_t>(range_) ? position - range_ : 0;
    const std::size_t to = std::min(own.size() - 1, position + static_cast<std::size_t>(range_));

    std::vector<std::size_t> candidates;
    for (std::size_t i = from; i <= to; ++i) {
        if (i == position) continue;
        Unit* unit = own.at(i);
        if (unit->isAlive() && unit->hp() < unit->maxHp() &&
            dynamic_cast<IHealable*>(unit) != nullptr) {
            candidates.push_back(i);
        }
    }
    if (candidates.empty()) return result;

    Unit* patient = own.at(candidates[rng() % candidates.size()]);
    result.performed = true;
    result.target = patient->name();
    result.restored = dynamic_cast<IHealable*>(patient)->heal(power_);
    return result;
}
