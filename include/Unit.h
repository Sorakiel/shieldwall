#pragma once
#include <memory>
#include <string>

enum class UnitKind { Light, Heavy, Archer };

class Unit {
public:
    virtual ~Unit() = default;                          // без этого - утечки
    virtual std::unique_ptr<Unit> clone() const = 0;    // для копий армий и сохранений
    virtual UnitKind kind() const = 0;
    virtual int meleeAttack() const = 0;

    const std::string& name() const { return name_; }
    int  hp()      const { return hp_; }
    int  maxHp()   const { return maxHp_; }
    int  defense() const { return defense_; }
    int  cost()    const { return cost_; }
    bool isAlive() const { return hp_ > 0; }
    void takeDamage(int dmg) { hp_ -= dmg; }

protected:
    Unit(std::string name, int maxHp, int defense, int cost)
        : name_(std::move(name)), maxHp_(maxHp), hp_(maxHp), defense_(defense), cost_(cost) {}

    std::string name_;
    int maxHp_ = 0, hp_ = 0, defense_ = 0, cost_ = 0;
};

class LightUnit : public Unit {
public:
    LightUnit(std::string name, int maxHp, int melee, int defense, int cost);
    std::unique_ptr<Unit> clone() const override;
    UnitKind kind() const override { return UnitKind::Light; }
    int meleeAttack() const override { return melee_; }
private:
    int melee_ = 0;
};

class HeavyUnit : public Unit {
public:
    HeavyUnit(std::string name, int maxHp, int melee, int defense, int cost);
    std::unique_ptr<Unit> clone() const override;
    UnitKind kind() const override { return UnitKind::Heavy; }
    int meleeAttack() const override { return melee_; }
private:
    int melee_ = 0;
};

class Archer : public Unit {
public:
    Archer(std::string name, int maxHp, int melee, int ranged, int range, int defense, int cost);
    std::unique_ptr<Unit> clone() const override;
    UnitKind kind() const override { return UnitKind::Archer; }
    int meleeAttack()  const override { return melee_; }
    int rangedAttack() const { return ranged_; }
    int range()        const { return range_; }
private:
    int melee_ = 0, ranged_ = 0, range_ = 0;
};
