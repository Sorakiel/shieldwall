#pragma once
#include <memory>
#include <algorithm>
#include <string>
#include "Abilities.h"

enum class UnitKind { Light, Heavy, Archer, Mage, Healer };

class Unit {
public:
    virtual ~Unit() = default;                          // без этого - утечки
    virtual std::unique_ptr<Unit> clone() const = 0;    // для копий армий и сохранений
    virtual UnitKind kind() const = 0;
    virtual int meleeAttack() const = 0;

    // Аксессоры виртуальные: декоратор и адаптер подменяют их, не трогая движок
    virtual const std::string& name() const { return name_; }
    virtual int  hp()      const { return hp_; }
    virtual int  maxHp()   const { return maxHp_; }
    virtual int  defense() const { return defense_; }
    virtual int  cost()    const { return cost_; }
    virtual bool isAlive() const { return hp() > 0; }
    virtual void takeDamage(int dmg) { hp_ -= dmg; }

protected:
    Unit(std::string name, int maxHp, int defense, int cost)
        : name_(std::move(name)), maxHp_(maxHp), hp_(maxHp), defense_(defense), cost_(cost) {}

    // Восстановление hp для наследников, которые реализуют IHealable.
    // Мёртвого не лечим, выше maxHp не поднимаем.
    int restore(int amount) {
        if (amount <= 0 || hp_ <= 0) return 0;
        const int before = hp_;
        hp_ = std::min(maxHp_, hp_ + amount);
        return hp_ - before;
    }

    std::string name_;
    int maxHp_ = 0, hp_ = 0, defense_ = 0, cost_ = 0;
};

class LightUnit : public Unit, public IClonable, public IHealable {
public:
    LightUnit(std::string name, int maxHp, int melee, int defense, int cost);
    std::unique_ptr<Unit> clone() const override;
    UnitKind kind() const override { return UnitKind::Light; }
    int meleeAttack() const override { return melee_; }
    std::unique_ptr<Unit> makeCopy() const override { return clone(); }
    int heal(int amount) override { return restore(amount); }
private:
    int melee_ = 0;
};

class HeavyUnit : public Unit, public IHealable {
public:
    HeavyUnit(std::string name, int maxHp, int melee, int defense, int cost);
    std::unique_ptr<Unit> clone() const override;
    UnitKind kind() const override { return UnitKind::Heavy; }
    int meleeAttack() const override { return melee_; }
    int heal(int amount) override { return restore(amount); }
private:
    int melee_ = 0;
};

class Archer : public Unit, public IClonable, public IHealable {
public:
    Archer(std::string name, int maxHp, int melee, int ranged, int range, int defense, int cost);
    std::unique_ptr<Unit> clone() const override;
    UnitKind kind() const override { return UnitKind::Archer; }
    int meleeAttack()  const override { return melee_; }
    int rangedAttack() const { return ranged_; }
    int range()        const { return range_; }
    std::unique_ptr<Unit> makeCopy() const override { return clone(); }
    int heal(int amount) override { return restore(amount); }
private:
    int melee_ = 0, ranged_ = 0, range_ = 0;
};
