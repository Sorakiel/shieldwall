#include "Menu.h"
#include <algorithm>
#include <limits>
#include <random>
#include "ArmyGenerator.h"
#include "BattleRunner.h"

void Menu::run() {
    if (catalog_.specs().empty()) {
        throw DataError("В каталоге нет типов юнитов");
    }
    const auto cheapest = std::min_element(catalog_.specs().begin(), catalog_.specs().end(),
        [](const UnitSpec& a, const UnitSpec& b) { return a.cost < b.cost; });
    const auto minBudget = static_cast<std::uint32_t>(cheapest->cost);
    ArmyGenerator generator(catalog_);
    Army a, b;
    std::mt19937 armyRng;
    std::uint32_t seed = 0;
    int budget = 0;
    State state = State::Setup;

    ui_.message("Shieldwall — автобой двух армий");
    while (state != State::Exit) {
        switch (state) {
        case State::Setup: {
            ui_.setStage(ViewStage::Setup);
            const auto limit = ui_.readNumber("Лимит цены каждой армии: ", minBudget,
                                              std::numeric_limits<int>::max());
            if (!limit) return;
            const auto enteredSeed = ui_.readNumber("Seed (например, 42): ", 0,
                                                    std::numeric_limits<std::uint32_t>::max());
            if (!enteredSeed) return;
            budget = static_cast<int>(*limit);
            seed = *enteredSeed;
            armyRng.seed(seed);
            state = State::Recruitment;
            break;
        }
        case State::Recruitment: {
            ui_.setStage(ViewStage::Recruitment);
            a = generator.generate(budget, armyRng);
            b = generator.generate(budget, armyRng);
            ui_.message("\nЛимит: " + std::to_string(budget) + ", seed: " + std::to_string(seed));
            ui_.showArmies(a, b);
            const auto choice = ui_.choose({{1, "К бою"}, {2, "Сгенерировать заново"},
                                            {3, "Изменить настройки"}, {0, "Выход"}});
            if (!choice) return;
            switch (*choice) {
            case 0: state = State::Exit; break;
            case 1: state = State::Battle; break;
            case 2: break;
            case 3: state = State::Setup; break;
            }
            break;
        }
        case State::Battle: {
            ui_.setStage(ViewStage::Battle);
            BattleEngine engine(a, b, seed);
            // У движка пока закрытый rng_. Отдельный поток с тем же seed сохраняет
            // воспроизводимость текста и не меняет исход при добавлении строк лога.
            std::mt19937 logRng(seed);
            BattleRunner(ui_).run(engine, a, b, logRng);
            state = State::Result;
            break;
        }
        case State::Result: {
            ui_.setStage(ViewStage::Result);
            const auto choice = ui_.choose({{1, "Новая партия"}, {0, "Выход"}});
            if (!choice) return;
            state = *choice == 1 ? State::Setup : State::Exit;
            break;
        }
        case State::Exit:
            break;
        }
    }
}
