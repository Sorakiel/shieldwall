#include "ArmyFactory.h"
#include <algorithm>
#include <vector>
#include "ArmyGenerator.h"

Army CompactArmyFactory::createArmy(int costLimit, std::mt19937& rng) const {
    std::vector<const UnitSpec*> byCost;
    for (const UnitSpec& spec : catalog_.specs()) byCost.push_back(&spec);
    // stable_sort: при равной цене остаётся порядок каталога, результат не зависит от платформы
    std::stable_sort(byCost.begin(), byCost.end(),
                     [](const UnitSpec* a, const UnitSpec* b) { return a->cost > b->cost; });

    Army army;
    bool boughtInRound = true;
    while (boughtInRound) {
        boughtInRound = false;
        for (const UnitSpec* spec : byCost) {
            if (ArmyGenerator::tryBuy(army, *spec, ArmyGenerator::randomName(rng), costLimit)) {
                boughtInRound = true;
            }
        }
    }
    return army;
}

Army RandomArmyFactory::createArmy(int costLimit, std::mt19937& rng) const {
    return ArmyGenerator(catalog_).generate(costLimit, rng);
}

std::unique_ptr<ArmyFactory> makeArmyFactory(ArmyAlgorithm algorithm, const UnitCatalog& catalog) {
    if (algorithm == ArmyAlgorithm::Compact) return std::make_unique<CompactArmyFactory>(catalog);
    return std::make_unique<RandomArmyFactory>(catalog);
}
