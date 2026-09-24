#include "UnitFactory.h"

std::unique_ptr<Unit> UnitFactory::create(const UnitSpec& spec, std::string name) {
    switch (spec.kind) {
        case UnitKind::Light:
            return std::make_unique<LightUnit>(std::move(name), spec.maxHp, spec.melee,
                                               spec.defense, spec.cost);
        case UnitKind::Heavy:
            return std::make_unique<HeavyUnit>(std::move(name), spec.maxHp, spec.melee,
                                               spec.defense, spec.cost);
        case UnitKind::Archer:
            return std::make_unique<Archer>(std::move(name), spec.maxHp, spec.melee, spec.ranged,
                                            spec.range, spec.defense, spec.cost);
    }
    throw DataError("фабрика не знает такого типа юнита");
}

std::unique_ptr<Unit> UnitFactory::create(const std::string& kindToken, const UnitSpec& stats,
                                          std::string name) {
    UnitSpec spec = stats;
    spec.kind = dataformat::parseKind(kindToken);
    dataformat::validateSpec(spec);
    return create(spec, std::move(name));
}
