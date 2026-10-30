#include "Mage.h"
#include <vector>

Mage::Mage(std::string name, int maxHp, int melee, int power, int range, int defense, int cost)
    : Unit(std::move(name), maxHp, defense, cost), melee_(melee), power_(power), range_(range) {}

std::unique_ptr<Unit> Mage::clone() const {
    return std::make_unique<Mage>(*this);
}

Mage::CastResult Mage::cast(Army& own, std::size_t position, std::mt19937& rng) const {
    CastResult result;
    if (position == 0 || position >= own.size()) return result;   // первый ряд не колдует

    const std::size_t from = position > static_cast<std::size_t>(range_) ? position - range_ : 0;
    const std::size_t to = std::min(own.size() - 1, position + static_cast<std::size_t>(range_));

    std::vector<std::size_t> candidates;
    for (std::size_t i = from; i <= to; ++i) {
        if (i == position) continue;
        Unit* unit = own.at(i);
        if (unit->isAlive() && dynamic_cast<IClonable*>(unit) != nullptr) {
            candidates.push_back(i);
        }
    }
    if (candidates.empty()) return result;

    // Остаток от деления, а не uniform_int_distribution: результат одинаков на всех платформах
    Unit* victim = own.at(candidates[rng() % candidates.size()]);
    result.performed = true;
    result.target = victim->name();

    if (static_cast<int>(rng() % 100) >= power_) return result;   // сила = шанс успеха в %

    own.insert(position, dynamic_cast<IClonable*>(victim)->makeCopy());
    result.success = true;
    return result;
}
