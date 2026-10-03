#include "Menu.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <limits>
#include <memory>
#include <random>
#include "ArmyBuilder.h"
#include "ArmyGenerator.h"
#include "BattleReplay.h"
#include "BattleRunner.h"
#include "EventFormatter.h"
#include "SaveSlots.h"

namespace {

constexpr std::size_t savesPerPage = 6;

std::string foldAscii(std::string value) {
    for (auto& character : value) if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
    return value;
}

std::string saveLabel(const SaveSummary& summary) {
    const std::string name = summary.name == SaveSlots::AutosaveName ?
        "Автосохранение" : std::filesystem::path(summary.name).u8string();
    if (!summary.valid) return name + "\nПовреждён: " + summary.error;
    const std::string phase = summary.phase == SavePhase::Recruitment ? "Закупка" :
        summary.phase == SavePhase::Battle ? "Бой" : "Итог";
    std::tm date{};
#ifdef _WIN32
    const bool hasDate = localtime_s(&date, &summary.modified) == 0;
#else
    const bool hasDate = localtime_r(&summary.modified, &date) != nullptr;
#endif
    std::ostringstream text;
    text << name << '\n' << phase << ", ход " << summary.turn
         << " | лимит " << summary.costLimit << " | seed " << summary.seed << " | ";
    if (hasDate) text << std::put_time(&date, "%d.%m.%Y %H:%M");
    else text << "дата неизвестна";
    return text.str();
}

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
    State loadReturn = State::Welcome;
    bool resultSaved = false;
    const auto parent = std::filesystem::path(autosavePath_).parent_path();
    const std::string saveDir = parent.empty() ? "." : parent.string();

    auto store = [&](const std::string& path, SavePhase phase, int turn) {
        SaveData snapshot = data;
        snapshot.phase = phase;
        snapshot.turn = turn;
        SaveService::saveToFile(snapshot, path);
    };
    auto checkpoint = [&](SavePhase phase, int turn) {
        data.phase = phase;
        data.turn = turn;
        try {
            store(autosavePath_, phase, turn);
        } catch (const DataError& error) {
            ui_.message(std::string("Автосохранение: ") + error.what());
        } catch (const std::filesystem::filesystem_error& error) {
            ui_.message(std::string("Не удалось создать папку автосохранения: ") + error.what());
        }
    };
    auto saveByName = [&](SavePhase phase, int turn) {
        while (true) {
            try {
                const auto suggestion = SaveSlots::suggestName(saveDir);
                const auto typed = ui_.readText("Имя сохранения [" + suggestion + "]: ", 64);
                if (!typed) return;
                const auto name = typed->empty() ? suggestion : *typed;
                // SaveSlots проверяет имя; путь из UTF-8 сохраняет кириллицу на Windows.
                SaveSlots::pathFor(saveDir, name);
                const auto path = parent / std::filesystem::u8path(name + ".txt");
                if (foldAscii(name) == SaveSlots::AutosaveName ||
                    foldAscii(path.filename().u8string()) ==
                    foldAscii(std::filesystem::path(autosavePath_).filename().u8string())) {
                    throw DataError("Это имя зарезервировано для автосохранения");
                }
                if (std::filesystem::exists(path)) {
                    std::error_code error;
                    if (std::filesystem::equivalent(path, autosavePath_, error)) {
                        throw DataError("Этот файл используется для автосохранения");
                    }
                    ui_.message("Сохранение «" + name + "» уже существует.");
                    const auto choice = ui_.choose({{1, "Перезаписать"}, {0, "Отмена", true, true}});
                    if (!choice || *choice == 0) return;
                }
                store(path.string(), phase, turn);
                ui_.message("Сохранено: " + name + " (ход " + std::to_string(turn) + ").");
                return;
            } catch (const DataError& error) {
                ui_.message(std::string("Не сохранено: ") + error.what());
            } catch (const std::filesystem::filesystem_error& error) {
                ui_.message(std::string("Не сохранено: ") + error.what());
                return;
            }
        }
    };
    auto selectSave = [&]() -> std::optional<std::string> {
        const auto saves = SaveSlots::list(saveDir);
        if (saves.empty()) {
            ui_.message("Сохранений нет.");
            return std::nullopt;
        }
        const auto nextPage = static_cast<std::uint32_t>(saves.size() + 1);
        const auto previousPage = nextPage + 1;
        std::size_t page = 0;
        while (true) {
            ui_.message("Сохранения — страница " + std::to_string(page + 1) + " из " +
                        std::to_string((saves.size() + savesPerPage - 1) / savesPerPage) + ".");
            std::vector<MenuChoice> choices;
            const auto end = std::min(saves.size(), (page + 1) * savesPerPage);
            for (std::size_t index = page * savesPerPage; index < end; ++index) {
                const auto label = saveLabel(saves[index]);
                choices.push_back({static_cast<std::uint32_t>(index + 1), label, saves[index].valid});
                if (!saves[index].valid) ui_.message(label);
            }
            choices.push_back({previousPage, "Назад по страницам", page > 0});
            choices.push_back({nextPage, "Далее", end < saves.size()});
            choices.push_back({0, "Отмена", true, true});
            const auto action = ui_.choose(choices);
            if (!action || *action == 0) return std::nullopt;
            if (*action == nextPage) {
                if (end < saves.size()) ++page;
            } else if (*action == previousPage) {
                if (page > 0) --page;
            } else if (*action <= saves.size() && *action > page * savesPerPage &&
                       *action <= end && saves[*action - 1].valid) {
                return saves[*action - 1].path;
            }
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
            ui_.message("Shieldwall — новая партия или загрузка сохранения.");
            const auto choice = ui_.choose({{1, "Новая партия"}, {2, "Загрузить"}, {0, "Выход"}});
            if (!choice || *choice == 0) return;
            loadReturn = State::Welcome;
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
            resultSaved = false;
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
                    choices.push_back({40, "Сохранить"});
                    choices.push_back({0, "В главное меню"});
                    const auto choice = ui_.choose(choices);
                    if (!choice) return;
                    if (*choice == 0) { completed = false; break; }
                    if (*choice == 40) {
                        data.armyA = builders[0]->units();
                        data.armyB = builders[1]->units();
                        saveByName(SavePhase::Recruitment, 0);
                        continue;
                    }
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
                                            {3, "Настройки"}, {4, "Загрузить"},
                                            {40, "Сохранить"}, {0, "Выход"}});
            if (!choice || *choice == 0) return;
            if (*choice == 40) { saveByName(SavePhase::Battle, 0); break; }
            if (*choice == 2) {
                clearSession(session);
                saveRecruitment();
                state = State::Recruitment;
            } else if (*choice == 3) state = State::Setup;
            else if (*choice == 4) { loadReturn = State::Ready; state = State::Load; }
            else state = State::Battle;
            break;
        }
        case State::Load: {
            try {
                const auto path = selectSave();
                if (!path) { state = loadReturn; break; }
                if (loadReturn == State::Ready || loadReturn == State::Result) {
                    const auto confirm = ui_.choose({{1, "Заменить текущую партию"},
                                                     {0, "Отмена", true, true}});
                    if (!confirm || *confirm == 0) { state = loadReturn; break; }
                }
                SaveData loaded = SaveService::loadFromFile(*path);
                if (loaded.phase == SavePhase::Recruitment) {
                    Builders restored;
                    restored[0] = std::make_unique<ArmyBuilder>(ArmyBuilder::restore(
                        catalog_, loaded.costLimit, loaded.armyA));
                    restored[1] = std::make_unique<ArmyBuilder>(ArmyBuilder::restore(
                        catalog_, loaded.costLimit, loaded.armyB));
                    const auto oldMode = mode;
                    const auto oldInput = privateInput;
                    if (!selectMode() || !selectInput()) {
                        mode = oldMode;
                        privateInput = oldInput;
                        state = loadReturn;
                        break;
                    }
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
                    if (!restored.engine->finished() && !selectMode()) {
                        state = loadReturn;
                        break;
                    }
                    clearSession(session);
                    session = std::move(restored);
                    state = session.engine->finished() ? State::Result : State::Battle;
                }
                data = std::move(loaded);
                armyRng.seed(data.seed);
                logRng.seed(data.seed);
                resultShown = false;
                resultSaved = false;
                ui_.message("Сохранение загружено. Сыграно ходов: " + std::to_string(data.turn));
            } catch (const DataError& error) {
                ui_.message(std::string("Загрузка: ") + error.what());
                state = loadReturn;
            } catch (const std::filesystem::filesystem_error& error) {
                ui_.message(std::string("Загрузка: ") + error.what());
                state = loadReturn;
            }
            break;
        }
        case State::Battle: {
            ui_.setStage(ViewStage::Battle);
            const auto outcome = BattleRunner(ui_).run(*session.engine, *session.armyA, *session.armyB,
                logRng, mode, session.turn, [&](int turn) { checkpoint(SavePhase::Battle, turn); },
                [&](int turn) { saveByName(SavePhase::Battle, turn); });
            session.turn = outcome.turn;
            mode = outcome.mode;
            resultShown = outcome.finished;
            state = outcome.finished ? State::Result : State::Welcome;
            break;
        }
        case State::Result: {
            if (!resultSaved) {
                checkpoint(SavePhase::Result, session.turn);
                resultSaved = true;
            }
            ui_.setStage(ViewStage::Result);
            ui_.setBattleProgress(session.turn, mode);
            if (!resultShown) {
                ui_.showArmies(*session.armyA, *session.armyB);
                ui_.showResult(session.engine->winner());
                resultShown = true;
            }
            const auto choice = ui_.choose({{1, "Новая партия"}, {2, "Загрузить"},
                                            {40, "Сохранить"}, {0, "Выход"}});
            if (!choice || *choice == 0) return;
            if (*choice == 40) { saveByName(SavePhase::Result, session.turn); break; }
            loadReturn = State::Result;
            state = *choice == 1 ? State::Setup : State::Load;
            break;
        }
        case State::Exit: return;
        }
    }
}
