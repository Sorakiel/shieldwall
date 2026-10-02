#include "Menu.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <memory>
#include <random>
#include "ArmyBuilder.h"
#include "ArmyGenerator.h"
#include "BattleReplay.h"
#include "BattleRunner.h"
#include "EventFormatter.h"

namespace {

using Builders = std::array<std::unique_ptr<ArmyBuilder>, 2>;

RecruitmentSnapshot recruitmentFrame(const Builders& builders, const UnitCatalog& catalog,
                                      int team, bool privateInput) {
    RecruitmentSnapshot frame;
    const ArmyBuilder& active = *builders[team];
    frame.activeTeam = team;
    frame.limit = active.limit();
    frame.spent = active.spent();
    frame.remaining = active.remaining();
    for (int index = 0; index < 2; ++index) {
        auto& army = frame.armies[index];
        army.name = EventFormatter::teamName(index);
        if (privateInput && index != team) continue;
        army.totalCost = builders[index]->spent();
        for (const auto& record : builders[index]->units()) {
            army.units.push_back({record.spec.kind, record.name, record.spec.maxHp,
                                  record.spec.maxHp, record.spec.cost});
        }
    }
    for (const auto& spec : catalog.specs()) {
        frame.offers.push_back({spec.kind, spec.cost, spec.maxHp, spec.melee, spec.ranged,
                                spec.range, spec.defense, active.canBuy(spec.kind)});
    }
    return frame;
}

std::uint32_t buyAction(UnitKind kind) {
    switch (kind) {
    case UnitKind::Light: return 1;
    case UnitKind::Heavy: return 2;
    case UnitKind::Archer: return 3;
    }
    return 0;
}

const char* buildError(BuildResult result) {
    switch (result) {
    case BuildResult::Ok: return "Состав обновлён.";
    case BuildResult::NotEnoughBudget: return "Недостаточно бюджета для этого бойца.";
    case BuildResult::UnknownKind: return "В каталоге нет этого типа бойца.";
    case BuildResult::BadPosition: return "Такой позиции в строю нет.";
    }
    return "Не удалось изменить состав.";
}

void clearSession(BattleSession& session) {
    // Движок уничтожаем первым: он хранит ссылки на армии.
    session.engine.reset();
    session.armyA.reset();
    session.armyB.reset();
    session.turn = 0;
}

}  // namespace

void Menu::run() {
    if (catalog_.specs().empty()) throw DataError("В каталоге нет типов юнитов");
    const auto cheapest = std::min_element(catalog_.specs().begin(), catalog_.specs().end(),
        [](const UnitSpec& a, const UnitSpec& b) { return a.cost < b.cost; });
    const auto minBudget = static_cast<std::uint32_t>(cheapest->cost);
    ArmyGenerator generator(catalog_);
    Builders builders;
    BattleSession session;
    SaveData data;
    std::mt19937 armyRng, logRng;
    BattleMode mode = BattleMode::Automatic;
    bool privateInput = false;
    bool resultShown = false;
    State state = State::Welcome;

    auto checkpoint = [&](SavePhase phase, int turn) {
        data.phase = phase;
        data.turn = turn;
        try {
            SaveService::saveToFile(data, autosavePath_);
        } catch (const DataError& error) {
            ui_.message(std::string("Автосохранение: ") + error.what());
        } catch (const std::filesystem::filesystem_error& error) {
            ui_.message(std::string("Не удалось создать папку автосохранения: ") + error.what());
        }
    };
    auto saveRecruitment = [&] {
        data.armyA = builders[0]->units();
        data.armyB = builders[1]->units();
        checkpoint(SavePhase::Recruitment, 0);
    };
    auto selectMode = [&] {
        ui_.message("Выберите управление ходами.");
        const auto choice = ui_.choose({{1, "Автобой"}, {2, "По одному ходу"}});
        if (!choice) return false;
        mode = *choice == 2 ? BattleMode::Manual : BattleMode::Automatic;
        return true;
    };
    auto selectInput = [&] {
        ui_.message("Как игроки набирают армии?");
        const auto choice = ui_.choose({{1, "Общий экран"}, {2, "Раздельный ввод"}});
        if (!choice) return false;
        privateInput = *choice == 2;
        return true;
    };

    while (state != State::Exit) {
        switch (state) {
        case State::Welcome: {
            ui_.setStage(ViewStage::Setup);
            ui_.message("Shieldwall — новая партия или продолжение автосохранения.");
            const auto choice = ui_.choose({{1, "Новая партия"}, {2, "Загрузить"}, {0, "Выход"}});
            if (!choice || *choice == 0) return;
            state = *choice == 2 ? State::Load : State::Setup;
            break;
        }
        case State::Setup: {
            ui_.setStage(ViewStage::Setup);
            const auto budget = ui_.readNumber("Лимит цены каждой армии: ", minBudget,
                                               std::numeric_limits<int>::max());
            if (!budget) return;
            const auto seed = ui_.readNumber("Seed (например, 42): ", 0,
                                             std::numeric_limits<std::uint32_t>::max());
            if (!seed || !selectMode() || !selectInput()) return;
            clearSession(session);
            data = {};
            data.costLimit = static_cast<int>(*budget);
            data.seed = *seed;
            armyRng.seed(data.seed);
            logRng.seed(data.seed);
            builders[0] = std::make_unique<ArmyBuilder>(catalog_, data.costLimit);
            builders[1] = std::make_unique<ArmyBuilder>(catalog_, data.costLimit);
            resultShown = false;
            saveRecruitment();
            state = State::Recruitment;
            break;
        }
        case State::Recruitment: {
            ui_.setStage(ViewStage::Recruitment);
            bool completed = true;
            for (int team = 0; team < 2 && completed; ++team) {
                if (privateInput && !ui_.handoff(team)) return;
                ui_.showRecruitment(recruitmentFrame(builders, catalog_, team, privateInput));
                ui_.message(std::string("Набор армии: ") + EventFormatter::teamName(team));
                const auto method = ui_.choose({{1, "Случайный набор"}, {2, "Ручная закупка"},
                                                {0, "В главное меню"}});
                if (!method) return;
                if (*method == 0) { completed = false; break; }
                if (*method == 1) {
                    Army generated = generator.generate(data.costLimit, armyRng);
                    builders[team] = std::make_unique<ArmyBuilder>(ArmyBuilder::restore(
                        catalog_, data.costLimit, SaveService::snapshot(generated)));
                    saveRecruitment();
                    continue;
                }

                bool editing = true;
                while (editing) {
                    const auto frame = recruitmentFrame(builders, catalog_, team, privateInput);
                    ui_.showRecruitment(frame);
                    std::vector<MenuChoice> choices;
                    for (const auto& offer : frame.offers) {
                        choices.push_back({buyAction(offer.kind),
                            std::string("Купить: ") + unitKindName(offer.kind), offer.enabled});
                    }
                    const bool hasUnits = !builders[team]->units().empty();
                    choices.push_back({20, "Убрать бойца", hasUnits});
                    choices.push_back({21, "Переставить", builders[team]->units().size() > 1});
                    choices.push_back({30, "Армия готова", hasUnits});
                    choices.push_back({0, "В главное меню"});
                    const auto choice = ui_.choose(choices);
                    if (!choice) return;
                    if (*choice == 0) { completed = false; break; }
                    if (*choice == 30) {
                        if (!hasUnits) { ui_.message("Сначала купите хотя бы одного бойца."); continue; }
                        editing = false;
                        continue;
                    }

                    BuildResult outcome = BuildResult::UnknownKind;
                    if (*choice >= 1 && *choice <= 3) {
                        const UnitKind kind = *choice == 1 ? UnitKind::Light :
                                              *choice == 2 ? UnitKind::Heavy : UnitKind::Archer;
                        outcome = builders[team]->buy(kind, armyRng);
                    } else if (*choice == 20 || *choice == 21) {
                        if (!hasUnits) { ui_.message("Строй пока пуст."); continue; }
                        const auto size = static_cast<std::uint32_t>(builders[team]->units().size());
                        const auto from = ui_.readNumber("Позиция бойца (с 1): ", 1, size);
                        if (!from) return;
                        if (*choice == 20) outcome = builders[team]->remove(*from - 1);
                        else {
                            const auto to = ui_.readNumber("Новая позиция (с 1): ", 1, size);
                            if (!to) return;
                            outcome = builders[team]->move(*from - 1, *to - 1);
                        }
                    }
                    ui_.message(buildError(outcome));
                    if (outcome == BuildResult::Ok) saveRecruitment();
                }
            }
            if (!completed) { state = State::Welcome; break; }
            clearSession(session);
            session.armyA = std::make_unique<Army>(builders[0]->build());
            session.armyB = std::make_unique<Army>(builders[1]->build());
            session.engine = std::make_unique<BattleEngine>(*session.armyA, *session.armyB, data.seed);
            // Эти копии не меняются после начала боя.
            data.armyA = builders[0]->units();
            data.armyB = builders[1]->units();
            checkpoint(SavePhase::Battle, 0);
            state = State::Ready;
            break;
        }
        case State::Ready: {
            ui_.setStage(ViewStage::Recruitment);
            ui_.message("Лимит: " + std::to_string(data.costLimit) + ", seed: " +
                        std::to_string(data.seed) + ". Обе армии готовы.");
            ui_.showArmies(*session.armyA, *session.armyB);
            const auto choice = ui_.choose({{1, "К бою"}, {2, "Изменить армии"},
                                            {3, "Настройки"}, {4, "Загрузить"}, {0, "Выход"}});
            if (!choice || *choice == 0) return;
            if (*choice == 2) {
                clearSession(session);
                saveRecruitment();
                state = State::Recruitment;
            } else if (*choice == 3) state = State::Setup;
            else if (*choice == 4) state = State::Load;
            else state = State::Battle;
            break;
        }
        case State::Load: {
            try {
                SaveData loaded = SaveService::loadFromFile(autosavePath_);
                if (loaded.phase == SavePhase::Recruitment) {
                    Builders restored;
                    restored[0] = std::make_unique<ArmyBuilder>(ArmyBuilder::restore(
                        catalog_, loaded.costLimit, loaded.armyA));
                    restored[1] = std::make_unique<ArmyBuilder>(ArmyBuilder::restore(
                        catalog_, loaded.costLimit, loaded.armyB));
                    if (!selectMode() || !selectInput()) return;
                    clearSession(session);
                    builders = std::move(restored);
                    state = State::Recruitment;
                } else {
                    if (loaded.armyA.empty() || loaded.armyB.empty()) {
                        throw DataError("сохранение боя должно содержать две стартовые армии");
                    }
                    BattleSession restored = BattleReplay::replay(loaded);
                    if (loaded.phase == SavePhase::Result && !restored.engine->finished()) {
                        throw DataError("сохранение результата содержит незавершённый бой");
                    }
                    if (!restored.engine->finished() && !selectMode()) return;
                    clearSession(session);
                    session = std::move(restored);
                    state = session.engine->finished() ? State::Result : State::Battle;
                }
                data = std::move(loaded);
                armyRng.seed(data.seed);
                logRng.seed(data.seed);
                resultShown = false;
                ui_.message("Автосохранение загружено. Сыграно ходов: " + std::to_string(data.turn));
            } catch (const DataError& error) {
                ui_.message(std::string("Загрузка: ") + error.what());
                state = State::Welcome;
            } catch (const std::filesystem::filesystem_error& error) {
                ui_.message(std::string("Загрузка: ") + error.what());
                state = State::Welcome;
            }
            break;
        }
        case State::Battle: {
            ui_.setStage(ViewStage::Battle);
            const auto outcome = BattleRunner(ui_).run(*session.engine, *session.armyA, *session.armyB,
                logRng, mode, session.turn, [&](int turn) { checkpoint(SavePhase::Battle, turn); });
            session.turn = outcome.turn;
            mode = outcome.mode;
            resultShown = outcome.finished;
            state = outcome.finished ? State::Result : State::Welcome;
            break;
        }
        case State::Result: {
            checkpoint(SavePhase::Result, session.turn);
            ui_.setStage(ViewStage::Result);
            ui_.setBattleProgress(session.turn, mode);
            if (!resultShown) {
                ui_.showArmies(*session.armyA, *session.armyB);
                ui_.showResult(session.engine->winner());
                resultShown = true;
            }
            const auto choice = ui_.choose({{1, "Новая партия"}, {2, "Загрузить"}, {0, "Выход"}});
            if (!choice || *choice == 0) return;
            state = *choice == 1 ? State::Setup : State::Load;
            break;
        }
        case State::Exit: return;
        }
    }
}
