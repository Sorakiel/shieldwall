#pragma once
#include <random>
#include <string>
#include "Army.h"
#include "UnitCatalog.h"

class ArmyGenerator {
public:
    explicit ArmyGenerator(UnitCatalog catalog) : catalog_(std::move(catalog)) {}

    // Добирает случайных юнитов, пока в лимит помещается хотя бы один тип.
    // Генератор чисел берётся по ссылке: копия повторила бы ту же последовательность,
    // а общий mt19937 - условие воспроизводимости боя и сохранений.
    Army generate(int costLimit, std::mt19937& rng) const;

    // Единственное место, где записано правило лимита цены.
    // Им же должна пользоваться ручная закупка, а не своей копией условия.
    static bool canAfford(int spent, int unitCost, int costLimit);

    // Ручная закупка: покупает юнита в конец строя, если он помещается в лимит.
    // Возвращает false и ничего не меняет, если не помещается.
    static bool tryBuy(Army& army, const UnitSpec& spec, std::string name, int costLimit);

private:
    UnitCatalog catalog_;
};
