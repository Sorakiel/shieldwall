#include "UnitDecorator.h"
#include <stdexcept>

const char* buffName(BuffKind buff) {
    switch (buff) {
    case BuffKind::Helmet: return "шлем";
    case BuffKind::Shield: return "щит";
    case BuffKind::Spear: return "копьё";
    case BuffKind::Horse: return "конь";
    }
    return "бонус";
}

UnitDecorator::UnitDecorator(std::unique_ptr<Unit> inner, BuffKind buff)
    : Unit(inner ? inner->name() : "", inner ? inner->maxHp() : 1,
           inner ? inner->defense() : 0, inner ? inner->cost() : 1),
      inner_(std::move(inner)) {
    if (!inner_) throw std::invalid_argument("декоратору нужен юнит");
    if (dynamic_cast<IHealable*>(inner_.get()) == nullptr) {
        throw std::invalid_argument("декорировать можно только юнита, которого можно лечить");
    }
    // Внешний вид: имя получает пометку вида «Гурм [шлем]»
    displayName_ = inner_->name() + " [" + buffName(buff) + "]";
}

std::unique_ptr<Unit> UnitDecorator::clone() const {
    return rewrap(inner_->clone());
}

int UnitDecorator::heal(int amount) {
    return dynamic_cast<IHealable*>(inner_.get())->heal(amount);
}

HelmetDecorator::HelmetDecorator(std::unique_ptr<Unit> inner)
    : UnitDecorator(std::move(inner), BuffKind::Helmet) {}
std::unique_ptr<Unit> HelmetDecorator::rewrap(std::unique_ptr<Unit> inner) const {
    return std::make_unique<HelmetDecorator>(std::move(inner));
}

ShieldDecorator::ShieldDecorator(std::unique_ptr<Unit> inner)
    : UnitDecorator(std::move(inner), BuffKind::Shield) {}
std::unique_ptr<Unit> ShieldDecorator::rewrap(std::unique_ptr<Unit> inner) const {
    return std::make_unique<ShieldDecorator>(std::move(inner));
}

SpearDecorator::SpearDecorator(std::unique_ptr<Unit> inner)
    : UnitDecorator(std::move(inner), BuffKind::Spear) {}
std::unique_ptr<Unit> SpearDecorator::rewrap(std::unique_ptr<Unit> inner) const {
    return std::make_unique<SpearDecorator>(std::move(inner));
}

HorseDecorator::HorseDecorator(std::unique_ptr<Unit> inner)
    : UnitDecorator(std::move(inner), BuffKind::Horse) {}
std::unique_ptr<Unit> HorseDecorator::rewrap(std::unique_ptr<Unit> inner) const {
    return std::make_unique<HorseDecorator>(std::move(inner));
}

std::unique_ptr<Unit> decorate(std::unique_ptr<Unit> unit, BuffKind buff) {
    switch (buff) {
    case BuffKind::Helmet: return std::make_unique<HelmetDecorator>(std::move(unit));
    case BuffKind::Shield: return std::make_unique<ShieldDecorator>(std::move(unit));
    case BuffKind::Spear: return std::make_unique<SpearDecorator>(std::move(unit));
    case BuffKind::Horse: return std::make_unique<HorseDecorator>(std::move(unit));
    }
    throw std::invalid_argument("неизвестный вид бонуса");
}

bool hasBuff(const Unit& unit, BuffKind buff) {
    const Unit* current = &unit;
    while (const auto* decorator = dynamic_cast<const UnitDecorator*>(current)) {
        if (decorator->buff() == buff) return true;
        current = &decorator->inner();
    }
    return false;
}
