#include "ArmyGenerator.h"
#include <string>
#include <vector>
#include "UnitFactory.h"

namespace {

const char* const NamePool[] = {
    "Вейн", "Гурм", "Тиль", "Хорд", "Брон", "Каэль", "Дарр", "Ульф",
    "Рейк", "Мирн", "Торв", "Ясень", "Орн", "Ларс", "Зэн", "Кай",
};
const std::size_t NamePoolSize = sizeof(NamePool) / sizeof(NamePool[0]);

// Индекс берётся через остаток от деления, а не через uniform_int_distribution:
// результат распределения зависит от реализации стандартной библиотеки,
// и один и тот же seed давал бы разные армии на Windows и на macOS.
std::size_t pickIndex(std::mt19937& rng, std::size_t count) {
    return static_cast<std::size_t>(rng() % count);
}

}  // namespace

std::string ArmyGenerator::randomName(std::mt19937& rng) {
    return NamePool[pickIndex(rng, NamePoolSize)];
}

bool ArmyGenerator::canAfford(int spent, int unitCost, int costLimit) {
    return unitCost > 0 && unitCost <= costLimit - spent;
}

bool ArmyGenerator::tryBuy(Army& army, const UnitSpec& spec, std::string name, int costLimit) {
    if (!canAfford(army.totalCost(), spec.cost, costLimit)) {
        return false;
    }
    army.add(UnitFactory::create(spec, std::move(name)));
    return true;
}

Army ArmyGenerator::generate(int costLimit, std::mt19937& rng) const {
    Army army;
    int spent = 0;

    while (true) {
        std::vector<const UnitSpec*> affordable;
        for (const UnitSpec& spec : catalog_.specs()) {
            if (canAfford(spent, spec.cost, costLimit)) {
                affordable.push_back(&spec);
            }
        }
        if (affordable.empty()) {
            break;
        }

        const UnitSpec& spec = *affordable[pickIndex(rng, affordable.size())];

        std::string name = randomName(rng);

        // Генератор покупает через ту же tryBuy, что и игрок руками
        if (!tryBuy(army, spec, std::move(name), costLimit)) {
            break;
        }
        spent += spec.cost;
    }
    return army;
}
