#include "MilitaryAcademy.h"
#include "ArmyGenerator.h"
#include "UnitCreator.h"

MilitaryAcademy::MilitaryAcademy(const UnitCatalog& catalog) {
    for (const UnitSpec& spec : catalog.specs()) {
        entries_.push_back({spec, UnitCreator::forKind(spec.kind).create(spec, "Образец")});
    }
}

const MilitaryAcademy::Entry* MilitaryAcademy::find(UnitKind kind) const {
    for (const Entry& entry : entries_) {
        if (entry.spec.kind == kind) return &entry;
    }
    return nullptr;
}

bool MilitaryAcademy::offers(UnitKind kind) const { return find(kind) != nullptr; }

const Unit* MilitaryAcademy::sample(UnitKind kind) const {
    const Entry* entry = find(kind);
    return entry ? entry->sample.get() : nullptr;
}

std::unique_ptr<Unit> MilitaryAcademy::enlist(UnitKind kind, std::string name) const {
    const Entry* entry = find(kind);
    if (entry == nullptr) throw DataError("в академии нет такого типа юнита");
    return UnitCreator::forKind(kind).create(entry->spec, std::move(name));
}

bool MilitaryAcademy::tryEnlist(Army& army, UnitKind kind, std::string name, int costLimit) const {
    const Entry* entry = find(kind);
    if (entry == nullptr) return false;
    if (!ArmyGenerator::canAfford(army.totalCost(), entry->spec.cost, costLimit)) return false;
    army.add(enlist(kind, std::move(name)));
    return true;
}
