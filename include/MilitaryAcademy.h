#pragma once
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "Army.h"
#include "Unit.h"
#include "UnitCatalog.h"

// «Военная академия»: хранит по одному образцу каждого доступного типа юнита и выдаёт
// новые экземпляры для ручной закупки. Экземпляры порождает Factory Method (UnitCreator).
// Образец нужен, чтобы показать игроку характеристики до покупки, не создавая лишнего.
class MilitaryAcademy {
public:
    struct Entry {
        UnitSpec spec;
        std::unique_ptr<Unit> sample;   // образец, в армию не попадает
    };

    explicit MilitaryAcademy(const UnitCatalog& catalog);

    const std::vector<Entry>& available() const { return entries_; }
    bool offers(UnitKind kind) const;
    const Unit* sample(UnitKind kind) const;   // nullptr, если такого типа нет

    // Новый экземпляр юнита. DataError, если типа нет в академии.
    std::unique_ptr<Unit> enlist(UnitKind kind, std::string name) const;

    // Ручная закупка в конец строя с проверкой лимита тем же правилом, что у генератора
    // (ArmyGenerator::canAfford). false и ничего не меняет, если не помещается или типа нет.
    bool tryEnlist(Army& army, UnitKind kind, std::string name, int costLimit) const;

private:
    const Entry* find(UnitKind kind) const;
    std::vector<Entry> entries_;
};
