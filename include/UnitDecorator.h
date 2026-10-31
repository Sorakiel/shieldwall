#pragma once
#include <memory>
#include <string>
#include "Unit.h"

enum class BuffKind { Helmet, Shield, Spear, Horse };

const char* buffName(BuffKind buff);   // «шлем», «щит», «копьё», «конь»

// Паттерн Decorator: обёртка над Unit добавляет бонус и меняет внешний вид (имя),
// но для движка остаётся обычным Unit. Обёртки можно складывать друг в друга.
// Декорируется только тот, кого можно лечить: обёртка пробрасывает IHealable внутрь.
class UnitDecorator : public Unit, public IHealable {
public:
    std::unique_ptr<Unit> clone() const override;
    UnitKind kind() const override { return inner_->kind(); }
    int meleeAttack() const override { return inner_->meleeAttack() + attackBonus(); }

    const std::string& name() const override { return displayName_; }
    int  hp() const override { return inner_->hp(); }
    int  maxHp() const override { return inner_->maxHp(); }
    int  defense() const override { return inner_->defense() + defenseBonus(); }
    int  cost() const override { return inner_->cost(); }
    void takeDamage(int dmg) override { inner_->takeDamage(dmg); }
    int  heal(int amount) override;

    virtual BuffKind buff() const = 0;
    const Unit& inner() const { return *inner_; }

protected:
    UnitDecorator(std::unique_ptr<Unit> inner, BuffKind buff);
    virtual int attackBonus() const { return 0; }
    virtual int defenseBonus() const { return 0; }
    // Та же обёртка вокруг другого юнита - нужна для clone()
    virtual std::unique_ptr<Unit> rewrap(std::unique_ptr<Unit> inner) const = 0;

private:
    std::unique_ptr<Unit> inner_;
    std::string displayName_;
};

class HelmetDecorator : public UnitDecorator {
public:
    explicit HelmetDecorator(std::unique_ptr<Unit> inner);
    BuffKind buff() const override { return BuffKind::Helmet; }
protected:
    int defenseBonus() const override { return 2; }
    std::unique_ptr<Unit> rewrap(std::unique_ptr<Unit> inner) const override;
};

class ShieldDecorator : public UnitDecorator {
public:
    explicit ShieldDecorator(std::unique_ptr<Unit> inner);
    BuffKind buff() const override { return BuffKind::Shield; }
protected:
    int defenseBonus() const override { return 3; }
    std::unique_ptr<Unit> rewrap(std::unique_ptr<Unit> inner) const override;
};

class SpearDecorator : public UnitDecorator {
public:
    explicit SpearDecorator(std::unique_ptr<Unit> inner);
    BuffKind buff() const override { return BuffKind::Spear; }
protected:
    int attackBonus() const override { return 4; }
    std::unique_ptr<Unit> rewrap(std::unique_ptr<Unit> inner) const override;
};

class HorseDecorator : public UnitDecorator {
public:
    explicit HorseDecorator(std::unique_ptr<Unit> inner);
    BuffKind buff() const override { return BuffKind::Horse; }
protected:
    int attackBonus() const override { return 2; }
    int defenseBonus() const override { return 1; }
    std::unique_ptr<Unit> rewrap(std::unique_ptr<Unit> inner) const override;
};

// Фабричная функция: нужная обёртка по виду бонуса
std::unique_ptr<Unit> decorate(std::unique_ptr<Unit> unit, BuffKind buff);

// Есть ли на юните (в любой из вложенных обёрток) бонус этого вида.
// Нужна, чтобы один и тот же бонус не вешался на юнита повторно.
bool hasBuff(const Unit& unit, BuffKind buff);
