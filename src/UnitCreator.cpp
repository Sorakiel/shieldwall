#include "UnitCreator.h"
#include "Healer.h"
#include "Mage.h"

namespace {

class LightCreator : public UnitCreator {
public:
    UnitKind kind() const override { return UnitKind::Light; }
    std::unique_ptr<Unit> create(const UnitSpec& s, std::string name) const override {
        return std::make_unique<LightUnit>(std::move(name), s.maxHp, s.melee, s.defense, s.cost);
    }
};

class HeavyCreator : public UnitCreator {
public:
    UnitKind kind() const override { return UnitKind::Heavy; }
    std::unique_ptr<Unit> create(const UnitSpec& s, std::string name) const override {
        return std::make_unique<HeavyUnit>(std::move(name), s.maxHp, s.melee, s.defense, s.cost);
    }
};

class ArcherCreator : public UnitCreator {
public:
    UnitKind kind() const override { return UnitKind::Archer; }
    std::unique_ptr<Unit> create(const UnitSpec& s, std::string name) const override {
        return std::make_unique<Archer>(std::move(name), s.maxHp, s.melee, s.ranged, s.range,
                                        s.defense, s.cost);
    }
};

// У мага и целителя в файле данных: выстрел = сила, дальность = радиус
class MageCreator : public UnitCreator {
public:
    UnitKind kind() const override { return UnitKind::Mage; }
    std::unique_ptr<Unit> create(const UnitSpec& s, std::string name) const override {
        return std::make_unique<Mage>(std::move(name), s.maxHp, s.melee, s.ranged, s.range,
                                      s.defense, s.cost);
    }
};

class HealerCreator : public UnitCreator {
public:
    UnitKind kind() const override { return UnitKind::Healer; }
    std::unique_ptr<Unit> create(const UnitSpec& s, std::string name) const override {
        return std::make_unique<Healer>(std::move(name), s.maxHp, s.melee, s.ranged, s.range,
                                        s.defense, s.cost);
    }
};

}  // namespace

// Новый тип юнита - новый создатель и одна строка здесь
const UnitCreator& UnitCreator::forKind(UnitKind kind) {
    static const LightCreator light;
    static const HeavyCreator heavy;
    static const ArcherCreator archer;
    static const MageCreator mage;
    static const HealerCreator healer;
    static const UnitCreator* const all[] = {&light, &heavy, &archer, &mage, &healer};
    for (const UnitCreator* creator : all) {
        if (creator->kind() == kind) return *creator;
    }
    throw DataError("фабрика не знает такого типа юнита");
}
