#include "UnitFactory.h"

namespace {

using Creator = std::unique_ptr<Unit> (*)(const UnitSpec&, std::string);

struct Entry {
    UnitKind kind;
    Creator create;
};

// Новый тип юнита - новая строка в таблице, код создания остальных типов не меняется
const Entry Entries[] = {
    {UnitKind::Light,
     [](const UnitSpec& s, std::string name) -> std::unique_ptr<Unit> {
         return std::make_unique<LightUnit>(std::move(name), s.maxHp, s.melee, s.defense, s.cost);
     }},
    {UnitKind::Heavy,
     [](const UnitSpec& s, std::string name) -> std::unique_ptr<Unit> {
         return std::make_unique<HeavyUnit>(std::move(name), s.maxHp, s.melee, s.defense, s.cost);
     }},
    {UnitKind::Archer,
     [](const UnitSpec& s, std::string name) -> std::unique_ptr<Unit> {
         return std::make_unique<Archer>(std::move(name), s.maxHp, s.melee, s.ranged, s.range,
                                         s.defense, s.cost);
     }},
};

}  // namespace

std::unique_ptr<Unit> UnitFactory::create(const UnitSpec& spec, std::string name) {
    for (const Entry& entry : Entries) {
        if (entry.kind == spec.kind) {
            return entry.create(spec, std::move(name));
        }
    }
    throw DataError("фабрика не знает такого типа юнита");
}
