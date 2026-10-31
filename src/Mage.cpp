#include "Mage.h"
#include <vector>
#include "CloneIdentity.h"

namespace {
// Keep the concrete LightUnit/Archer type so healing, shooting and further
// cloning use the same interfaces as the source. The Unit interface is unchanged.
template <class Base> class NumberedClone final : public Base {
public:
    NumberedClone(const Base& source, std::size_t number) : Base(source) {
        this->name_ = cloneidentity::numbered(source.name(), number);
    }
    std::unique_ptr<Unit> clone() const override {
        return std::make_unique<NumberedClone>(*this);
    }
};
std::unique_ptr<Unit> numberedCopy(std::unique_ptr<Unit> copy, std::size_t number) {
    if (const auto* light = dynamic_cast<const LightUnit*>(copy.get()))
        return std::make_unique<NumberedClone<LightUnit>>(*light, number);
    if (const auto* archer = dynamic_cast<const Archer*>(copy.get()))
        return std::make_unique<NumberedClone<Archer>>(*archer, number);
    return copy;
}
} // namespace

Mage::Mage(std::string name, int maxHp, int melee, int power, int range, int defense, int cost)
    : Unit(std::move(name), maxHp, defense, cost), melee_(melee), power_(power), range_(range) {}

std::unique_ptr<Unit> Mage::clone() const {
    return std::make_unique<Mage>(*this);
}

Mage::CastResult Mage::cast(Army& own, std::size_t position, std::mt19937& rng) const {
    CastResult result;
    if (position == 0 || position >= own.size())
        return result; // первый ряд не колдует

    const std::size_t from = position > static_cast<std::size_t>(range_) ? position - range_ : 0;
    const std::size_t to = std::min(own.size() - 1, position + static_cast<std::size_t>(range_));

    std::vector<std::size_t> candidates;
    for (std::size_t i = from; i <= to; ++i) {
        if (i == position)
            continue;
        Unit* unit = own.at(i);
        if (unit->isAlive() && dynamic_cast<IClonable*>(unit) != nullptr) {
            candidates.push_back(i);
        }
    }
    if (candidates.empty())
        return result;

    // Остаток от деления, а не uniform_int_distribution: результат одинаков на всех платформах
    Unit* victim = own.at(candidates[rng() % candidates.size()]);
    result.performed = true;
    result.target = victim->name();

    if (static_cast<int>(rng() % 100) >= power_)
        return result; // сила = шанс успеха в %

    std::size_t number = clonesCreated_;
    for (std::size_t i = 0; i < own.size(); ++i) {
        number = std::max(number, cloneidentity::parse(own.at(i)->name()).number);
        if (const auto* mage = dynamic_cast<const Mage*>(own.at(i)))
            number = std::max(number, mage->clonesCreated_);
    }
    ++number;
    auto copy = numberedCopy(dynamic_cast<IClonable*>(victim)->makeCopy(), number);
    own.insert(position, std::move(copy));
    // Mages cannot be created during battle. Every surviving caster retains the
    // team counter, including numbers belonging to fallen clones or mages.
    for (std::size_t i = 0; i < own.size(); ++i)
        if (auto* mage = dynamic_cast<Mage*>(own.at(i)))
            mage->clonesCreated_ = number;
    result.success = true;
    return result;
}
