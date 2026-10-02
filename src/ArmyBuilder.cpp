#include "ArmyBuilder.h"
#include "ArmyGenerator.h"
#include "UnitFactory.h"

namespace {

const UnitSpec* findSpec(const UnitCatalog& catalog, UnitKind kind) {
    for (const UnitSpec& spec : catalog.specs()) {
        if (spec.kind == kind) {
            return &spec;
        }
    }
    return nullptr;
}

}  // namespace

ArmyBuilder::ArmyBuilder(UnitCatalog catalog, int costLimit)
    : catalog_(std::move(catalog)), costLimit_(costLimit) {}

bool ArmyBuilder::canBuy(UnitKind kind) const {
    const UnitSpec* spec = findSpec(catalog_, kind);
    return spec != nullptr && ArmyGenerator::canAfford(spent_, spec->cost, costLimit_);
}

BuildResult ArmyBuilder::buy(UnitKind kind, std::mt19937& rng) {
    const UnitSpec* spec = findSpec(catalog_, kind);
    if (spec == nullptr) {
        return BuildResult::UnknownKind;
    }
    // Правило лимита то же, что у генератора: копии условия нет
    if (!ArmyGenerator::canAfford(spent_, spec->cost, costLimit_)) {
        return BuildResult::NotEnoughBudget;
    }
    units_.push_back({*spec, ArmyGenerator::randomName(rng)});
    spent_ += spec->cost;
    return BuildResult::Ok;
}

BuildResult ArmyBuilder::remove(std::size_t index) {
    if (index >= units_.size()) {
        return BuildResult::BadPosition;
    }
    spent_ -= units_[index].spec.cost;
    units_.erase(units_.begin() + static_cast<std::ptrdiff_t>(index));
    return BuildResult::Ok;
}

BuildResult ArmyBuilder::move(std::size_t from, std::size_t to) {
    if (from >= units_.size() || to >= units_.size()) {
        return BuildResult::BadPosition;
    }
    UnitRecord record = std::move(units_[from]);
    units_.erase(units_.begin() + static_cast<std::ptrdiff_t>(from));
    units_.insert(units_.begin() + static_cast<std::ptrdiff_t>(to), std::move(record));
    return BuildResult::Ok;
}

Army ArmyBuilder::build() const {
    Army army;
    for (const UnitRecord& record : units_) {
        army.add(UnitFactory::create(record.spec, record.name));
    }
    return army;
}
