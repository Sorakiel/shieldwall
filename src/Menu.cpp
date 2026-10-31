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
#include "ArmyFactory.h"
#include "MilitaryAcademy.h"
#include "BattleReplay.h"
#include "BattleRunner.h"
#include "EventFormatter.h"
#include "SaveSlots.h"

namespace {

constexpr std::size_t savesPerPage = 6;

std::string foldAscii(std::string value) {
    for (auto& character : value)
        if (character >= 'A' && character <= 'Z')
            character += 'a' - 'A';
    return value;
}

std::string saveLabel(const SaveSummary& summary) {
    const std::string name = summary.name == SaveSlots::AutosaveName
                                 ? "Автосохранение"
                                 : std::filesystem::path(summary.name).u8string();
    if (!summary.valid)
        return name + "\nПовреждён: " + summary.error;
    const std::string phase = summary.phase == SavePhase::Recruitment ? "Закупка"
                              : summary.phase == SavePhase::Battle    ? "Бой"
                                                                      : "Итог";
    std::tm date{};
#ifdef _WIN32
    const bool hasDate = localtime_s(&date, &summary.modified) == 0;
#else
    const bool hasDate = localtime_r(&summary.modified, &date) != nullptr;
#endif
    std::ostringstream text;
    text << name << '\n'
         << phase << ", ход " << summary.turn << " | лимит " << summary.costLimit << " | seed "
         << summary.seed << " | ";
    if (hasDate)
        text << std::put_time(&date, "%d.%m.%Y %H:%M");
    else
        text << "дата неизвестна";
    return text.str();
}

using Builders = std::array<std::unique_ptr<ArmyBuilder>, 2>;

RecruitmentSnapshot recruitmentFrame(const Builders& builders, const MilitaryAcademy& academy, int team,
                                     bool privateInput) {
    RecruitmentSnapshot frame;
    const ArmyBuilder& active = *builders[team];
    frame.activeTeam = team;
    frame.limit = active.limit();
    frame.spent = active.spent();
    frame.remaining = active.remaining();
    for (int index = 0; index < 2; ++index) {
        auto& army = frame.armies[index];
        army.name = EventFormatter::teamName(index);
        if (privateInput && index != team)
            continue;
        army.totalCost = builders[index]->spent();
        for (const auto& record : builders[index]->units()) {
            army.units.push_back(
                {record.spec.kind, record.name, record.spec.maxHp, record.spec.maxHp, record.spec.cost});
        }
    }
    for (const auto& entry : academy.available()) {
        const Unit& sample = *entry.sample;
        const auto& spec = entry.spec;
        frame.offers.push_back({sample.kind(), sample.cost(), sample.maxHp(), sample.meleeAttack(),
                                spec.ranged, spec.range, sample.defense(), active.canBuy(spec.kind)});
    }
    return frame;
}

std::uint32_t buyAction(UnitKind kind) {
    switch (kind) {
    case UnitKind::Light:
        return 1;
    case UnitKind::Heavy:
        return 2;
    case UnitKind::Archer:
        return 3;
    case UnitKind::Mage:
        return 4;
    case UnitKind::Healer:
        return 5;
    }
    return 0;
}

BuildResult recruitFromAcademy(ArmyBuilder& draft, const MilitaryAcademy& academy, UnitKind kind,
                               std::mt19937& rng) {
    if (!academy.offers(kind))
        return BuildResult::UnknownKind;
    if (!draft.canBuy(kind))
        return BuildResult::NotEnoughBudget;
    Army recruited;
    if (!academy.tryEnlist(recruited, kind, ArmyGenerator::randomName(rng), draft.remaining())) {
        return BuildResult::NotEnoughBudget;
    }
    auto records = draft.units();
    auto added = SaveService::snapshot(recruited);
    records.push_back(std::move(added.front()));
    // Keep the existing serializable draft and all saved unit characteristics.
    draft = ArmyBuilder::restore(draft.catalog(), draft.limit(), std::move(records));
    return BuildResult::Ok;
}

const char* buildError(BuildResult result) {
    switch (result) {
    case BuildResult::Ok:
        return "";
    case BuildResult::NotEnoughBudget:
        return "Недостаточно бюджета для этого бойца.";
    case BuildResult::UnknownKind:
        return "В каталоге нет этого типа бойца.";
    case BuildResult::BadPosition:
        return "Такой позиции в строю нет.";
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

} // namespace

void Menu::run(const std::vector<std::string>& initialMessages) {
    if (catalog_.specs().empty())
        throw DataError("В каталоге нет типов юнитов");
    const auto cheapest =
        std::min_element(catalog_.specs().begin(), catalog_.specs().end(),
                         [](const UnitSpec& a, const UnitSpec& b) { return a.cost < b.cost; });
    const auto minBudget = static_cast<std::uint32_t>(cheapest->cost);
    MilitaryAcademy academy(catalog_);
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
    int setupStep = 0, recruitTeam = 0, handoffReturn = -1;
    std::uint32_t setupBudget = 120, setupSeed = 42;
    bool needHandoff = true, draftInitialized = false;
    RecruitmentScreen recruitScreen = RecruitmentScreen::Methods;
    ArmyAlgorithm generatedAlgorithm = ArmyAlgorithm::Random;
    std::vector<std::string> pendingMessages = initialMessages;
    auto showPendingMessages = [&] {
        for (const auto& text : pendingMessages)
            ui_.message(text);
        pendingMessages.clear();
    };
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
                if (!typed)
                    return;
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
                    if (!choice || *choice == 0)
                        return;
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
        bool deleting = false;
        std::size_t page = 0;
        while (true) {
            // Refresh after deletion and keep the last page within the new list.
            const auto saves = SaveSlots::list(saveDir);
            if (saves.empty()) {
                pendingMessages.push_back("Сохранений нет.");
                return std::nullopt;
            }
            page = std::min(page, (saves.size() - 1) / savesPerPage);
            const auto nextPage = static_cast<std::uint32_t>(saves.size() + 1);
            const auto previousPage = nextPage + 1;
            const auto toggleDelete = nextPage + 2;
            ui_.message(std::string(deleting ? "Выберите сохранение для удаления — " : "Сохранения — ") +
                        "страница " + std::to_string(page + 1) + " из " +
                        std::to_string((saves.size() + savesPerPage - 1) / savesPerPage) + ".");
            std::vector<MenuChoice> choices;
            const auto end = std::min(saves.size(), (page + 1) * savesPerPage);
            for (std::size_t index = page * savesPerPage; index < end; ++index) {
                const auto label = saveLabel(saves[index]);
                choices.push_back(
                    {static_cast<std::uint32_t>(index + 1), label, deleting || saves[index].valid});
                if (!saves[index].valid)
                    ui_.message(label);
            }
            choices.push_back({previousPage, "Назад по страницам", page > 0});
            choices.push_back({nextPage, "Далее", end < saves.size()});
            choices.push_back({toggleDelete, deleting ? "К загрузке" : "Удалить сохранения"});
            choices.push_back({0, "Назад", true, true});
            const auto action = ui_.choose(choices);
            if (!action || *action == 0)
                return std::nullopt;
            if (*action == toggleDelete) {
                deleting = !deleting;
                continue;
            }
            if (*action == nextPage) {
                if (end < saves.size())
                    ++page;
            } else if (*action == previousPage) {
                if (page > 0)
                    --page;
            } else if (*action <= saves.size() && *action > page * savesPerPage && *action <= end) {
                const auto& selected = saves[*action - 1];
                if (!deleting) {
                    if (selected.valid)
                        return selected.path;
                    continue;
                }
                ui_.message("Удалить сохранение «" + selected.name + "»? Это действие нельзя отменить.");
                if (selected.name == SaveSlots::AutosaveName) {
                    ui_.message("Автосохранение появится снова при следующем сохранении прогресса.");
                }
                const auto confirm = ui_.choose({{1, "Удалить сохранение"}, {0, "Отмена", true, true}});
                if (!confirm || *confirm == 0)
                    continue;
                const std::filesystem::path file(selected.path);
                // Delete only a .txt file enumerated directly in the saves directory.
                if (file.extension() != ".txt" || !std::filesystem::equivalent(file.parent_path(), saveDir)) {
                    throw DataError("Файл находится вне папки сохранений");
                }
                std::error_code error;
                const bool removed = std::filesystem::remove(file, error);
                ui_.message(error     ? "Не удалось удалить: " + error.message()
                            : removed ? "Сохранение удалено."
                                      : "Сохранение уже отсутствует.");
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
        const auto choice = ui_.choose({{1, "Автобой"},
                                        {2, "По одному ходу"},
                                        {uiAction::Back, "Назад", true, true},
                                        {0, "В главное меню"}});
        if (choice && (*choice == 1 || *choice == 2))
            mode = *choice == 2 ? BattleMode::Manual : BattleMode::Automatic;
        return choice;
    };
    auto selectInput = [&] {
        ui_.message("Как игроки набирают армии?");
        const auto choice = ui_.choose({{1, "Общий экран"},
                                        {2, "Раздельный ввод"},
                                        {uiAction::Back, "Назад", true, true},
                                        {0, "В главное меню"}});
        if (choice && (*choice == 1 || *choice == 2))
            privateInput = *choice == 2;
        return choice;
    };
    auto generateDraft = [&] {
        const auto factory = makeArmyFactory(generatedAlgorithm, catalog_);
        Army generated = factory->createArmy(data.costLimit, armyRng);
        builders[recruitTeam] = std::make_unique<ArmyBuilder>(
            ArmyBuilder::restore(catalog_, data.costLimit, SaveService::snapshot(generated)));
        saveRecruitment();
    };
    auto enterReady = [&] {
        clearSession(session);
        session.armyA = std::make_unique<Army>(builders[0]->build());
        session.armyB = std::make_unique<Army>(builders[1]->build());
        session.engine = std::make_unique<BattleEngine>(*session.armyA, *session.armyB, data.seed);
        data.armyA = builders[0]->units();
        data.armyB = builders[1]->units();
        checkpoint(SavePhase::Battle, 0);
        state = State::Ready;
    };
    auto startNewSetup = [&] {
        clearSession(session);
        builders = {};
        draftInitialized = false;
        setupStep = 0;
        setupBudget = 120;
        setupSeed = 42;
    };

    while (state != State::Exit) {
        switch (state) {
        case State::Welcome: {
            ui_.clearDisplay();
            ui_.setStage(ViewStage::MainMenu);
            ui_.message("Shieldwall — новая партия или загрузка сохранения.");
            showPendingMessages();
            const auto choice = ui_.choose({{1, "Новая партия"}, {2, "Загрузить"}, {0, "Выход"}});
            if (!choice || *choice == 0)
                return;
            loadReturn = State::Welcome;
            if (*choice == 1)
                startNewSetup();
            state = *choice == 2 ? State::Load : State::Setup;
            break;
        }
        case State::Setup: {
            ui_.setStage(ViewStage::Setup);
            bool cancelled = false;
            while (setupStep < 4 && !cancelled) {
                try {
                    if (setupStep < 2) {
                        const auto value =
                            setupStep == 0 ? ui_.readSetupNumber("Лимит цены каждой армии: ", minBudget,
                                                                 std::numeric_limits<int>::max(),
                                                                 NumberCancel::Previous, setupBudget)
                                           : ui_.readSetupNumber("Seed (например, 42): ", 0,
                                                                 std::numeric_limits<std::uint32_t>::max(),
                                                                 NumberCancel::Previous, setupSeed);
                        if (!value) {
                            cancelled = true;
                            break;
                        }
                        if (setupStep == 0)
                            setupBudget = *value;
                        else
                            setupSeed = *value;
                        ++setupStep;
                    } else {
                        const auto choice = setupStep == 2 ? selectMode() : selectInput();
                        if (!choice || *choice == 0) {
                            cancelled = true;
                            break;
                        }
                        if (*choice == uiAction::Back)
                            --setupStep;
                        else
                            ++setupStep;
                    }
                } catch (const ViewBack&) {
                    if (setupStep == 0)
                        cancelled = true;
                    else
                        --setupStep;
                }
            }
            if (cancelled) {
                state = State::Welcome;
                break;
            }
            const bool changedSeed = !draftInitialized || data.seed != setupSeed;
            const bool changedBudget = !draftInitialized || data.costLimit != static_cast<int>(setupBudget);
            clearSession(session);
            if (changedSeed) {
                armyRng.seed(setupSeed);
                logRng.seed(setupSeed);
            }
            if (changedBudget) {
                for (auto& builder : builders) {
                    const auto records = builder && builder->spent() <= static_cast<int>(setupBudget)
                                             ? builder->units()
                                             : std::vector<UnitRecord>{};
                    builder = std::make_unique<ArmyBuilder>(
                        ArmyBuilder::restore(catalog_, static_cast<int>(setupBudget), records));
                }
            }
            data.costLimit = static_cast<int>(setupBudget);
            data.seed = setupSeed;
            draftInitialized = true;
            resultShown = false;
            resultSaved = false;
            recruitTeam = 0;
            recruitScreen = RecruitmentScreen::Methods;
            needHandoff = true;
            handoffReturn = -1;
            saveRecruitment();
            state = State::Recruitment;
            break;
        }
        case State::Recruitment: {
            ui_.setStage(ViewStage::Recruitment);
            const int team = recruitTeam;
            if (needHandoff && privateInput) {
                try {
                    if (!ui_.handoff(team))
                        return;
                } catch (const ViewBack&) {
                    needHandoff = false;
                    if (handoffReturn < 0) {
                        setupStep = 3;
                        state = State::Setup;
                    } else if (handoffReturn == 2)
                        enterReady();
                    else {
                        recruitTeam = handoffReturn;
                        recruitScreen = RecruitmentScreen::Methods;
                    }
                    break;
                }
            }
            needHandoff = false;
            auto frame = recruitmentFrame(builders, academy, team, privateInput);
            frame.screen = recruitScreen;
            ui_.showRecruitment(frame);
            const bool hasUnits = !builders[team]->units().empty();
            std::vector<MenuChoice> choices;
            if (recruitScreen == RecruitmentScreen::Methods) {
                ui_.message(std::string("Набор армии: ") + EventFormatter::teamName(team));
                choices = {{1, "Случайный набор"}, {2, "Ручная закупка"}, {3, "Сжатый набор"}};
            } else {
                for (const auto& offer : frame.offers)
                    choices.push_back({buyAction(offer.kind),
                                       std::string("Купить: ") + unitKindName(offer.kind), offer.enabled});
            }
            // Console actions; SFML exposes these edits directly on the cards.
            choices.push_back({20, "Убрать бойца", hasUnits});
            choices.push_back({21, "Переставить", builders[team]->units().size() > 1});
            choices.push_back({uiAction::ClearArmy, "Очистить армию", hasUnits});
            choices.push_back({30, "Армия готова", hasUnits});
            choices.push_back({uiAction::Back, "Назад", true, true});
            choices.push_back({0, "В главное меню"});
            const auto choice = ui_.choose(choices);
            if (!choice)
                return;
            if (*choice == 0) {
                state = State::Welcome;
                break;
            }
            if (*choice == uiAction::Back) {
                if (recruitScreen != RecruitmentScreen::Methods)
                    recruitScreen = RecruitmentScreen::Methods;
                else if (team == 1) {
                    recruitTeam = 0;
                    needHandoff = true;
                    handoffReturn = 1;
                } else {
                    setupStep = 3;
                    state = State::Setup;
                }
                break;
            }
            if (*choice == uiAction::ClearArmy) {
                builders[team] = std::make_unique<ArmyBuilder>(catalog_, data.costLimit);
                saveRecruitment();
                break;
            }
            if (*choice == 30) {
                if (!hasUnits) {
                    ui_.message("Сначала наймите хотя бы одного бойца.");
                    break;
                }
                if (team == 0) {
                    recruitTeam = 1;
                    recruitScreen = RecruitmentScreen::Methods;
                    needHandoff = true;
                    handoffReturn = 0;
                    break;
                }
                enterReady();
                break;
            }
            if (recruitScreen == RecruitmentScreen::Methods &&
                (*choice == 1 || *choice == 2 || *choice == 3)) {
                if (*choice == 2)
                    recruitScreen = RecruitmentScreen::Academy;
                else if (*choice == 1 || *choice == 3) {
                    generatedAlgorithm = *choice == 3 ? ArmyAlgorithm::Compact : ArmyAlgorithm::Random;
                    generateDraft();
                }
                break;
            }
            BuildResult outcome = BuildResult::UnknownKind;
            if (*choice == uiAction::ArmyEdit) {
                if (const auto edit = ui_.takeArmyEdit())
                    outcome = edit->kind == ArmyEdit::Kind::Remove
                                  ? builders[team]->remove(edit->from)
                                  : builders[team]->move(edit->from, edit->to);
            } else {
                const auto purchase =
                    std::find_if(frame.offers.begin(), frame.offers.end(),
                                 [&](const RecruitmentOffer& o) { return buyAction(o.kind) == *choice; });
                if (purchase != frame.offers.end())
                    outcome = recruitFromAcademy(*builders[team], academy, purchase->kind, armyRng);
                else if (*choice == 20 || *choice == 21) {
                    const auto size = static_cast<std::uint32_t>(builders[team]->units().size());
                    if (size == 0)
                        break;
                    const auto from =
                        ui_.readNumberCancelable("Позиция бойца (с 1): ", 1, size, NumberCancel::Dialog);
                    if (!from)
                        break;
                    if (*choice == 20)
                        outcome = builders[team]->remove(*from - 1);
                    else {
                        const auto to =
                            ui_.readNumberCancelable("Новая позиция (с 1): ", 1, size, NumberCancel::Dialog);
                        if (!to)
                            break;
                        outcome = builders[team]->move(*from - 1, *to - 1);
                    }
                }
            }
            if (outcome == BuildResult::Ok)
                saveRecruitment();
            else
                ui_.message(buildError(outcome));
            break;
        }
        case State::Ready: {
            ui_.setStage(ViewStage::Recruitment);
            ui_.message("Лимит: " + std::to_string(data.costLimit) + ", seed: " + std::to_string(data.seed) +
                        ". Обе армии готовы.");
            ui_.showArmies(*session.armyA, *session.armyB);
            showPendingMessages();
            const auto choice =
                ui_.choose({{1, "К бою"}, {uiAction::Back, "Назад", true, true}, {0, "В главное меню"}});
            if (!choice)
                return;
            if (*choice == 0) {
                state = State::Welcome;
                break;
            }
            if (*choice == uiAction::Back) {
                clearSession(session);
                saveRecruitment();
                recruitTeam = 1;
                recruitScreen = RecruitmentScreen::Methods;
                needHandoff = true;
                handoffReturn = 2;
                state = State::Recruitment;
            } else if (*choice == 1)
                state = State::Battle;
            break;
        }
        case State::Load: {
            try {
                const auto path = selectSave();
                if (!path) {
                    state = loadReturn;
                    break;
                }
                if (loadReturn == State::Ready || loadReturn == State::Result) {
                    const auto confirm =
                        ui_.choose({{1, "Заменить текущую партию"}, {0, "Отмена", true, true}});
                    if (!confirm || *confirm == 0) {
                        state = loadReturn;
                        break;
                    }
                }
                SaveData loaded = SaveService::loadFromFile(*path);
                if (loaded.phase == SavePhase::Recruitment) {
                    Builders restored;
                    restored[0] = std::make_unique<ArmyBuilder>(
                        ArmyBuilder::restore(catalog_, loaded.costLimit, loaded.armyA));
                    restored[1] = std::make_unique<ArmyBuilder>(
                        ArmyBuilder::restore(catalog_, loaded.costLimit, loaded.armyB));
                    ui_.clearDisplay();
                    ui_.setStage(ViewStage::Setup);
                    clearSession(session);
                    builders = std::move(restored);
                    draftInitialized = true;
                    setupBudget = static_cast<std::uint32_t>(loaded.costLimit);
                    setupSeed = loaded.seed;
                    setupStep = 2;
                    state = State::Setup;
                } else {
                    if (loaded.armyA.empty() || loaded.armyB.empty()) {
                        throw DataError("сохранение боя должно содержать две стартовые армии");
                    }
                    BattleSession restored = BattleReplay::replay(loaded);
                    if (loaded.phase == SavePhase::Result && !restored.engine->finished()) {
                        throw DataError("сохранение результата содержит незавершённый бой");
                    }
                    ui_.clearDisplay();
                    resultShown = false;
                    ui_.setStage(ViewStage::Setup);
                    if (!restored.engine->finished()) {
                        const auto choice = selectMode();
                        if (choice && *choice == uiAction::Back) {
                            state = State::Load;
                            break;
                        }
                        if (!choice || *choice == 0) {
                            state = State::Welcome;
                            break;
                        }
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
                pendingMessages.push_back(std::string("Загрузка: ") + error.what());
                state = loadReturn;
            } catch (const std::filesystem::filesystem_error& error) {
                pendingMessages.push_back(std::string("Загрузка: ") + error.what());
                state = loadReturn;
            }
            break;
        }
        case State::Battle: {
            ui_.setStage(ViewStage::Battle);
            const auto outcome = BattleRunner(ui_).run(
                *session.engine, *session.armyA, *session.armyB, logRng, mode, session.turn,
                [&](int turn) { checkpoint(SavePhase::Battle, turn); },
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
            showPendingMessages();
            const auto choice =
                ui_.choose({{1, "Новая партия"}, {2, "Загрузить"}, {0, "В главное меню", true, true}});
            if (!choice)
                return;
            if (*choice == 0) {
                state = State::Welcome;
                break;
            }
            loadReturn = State::Result;
            if (*choice == 1)
                startNewSetup();
            state = *choice == 1 ? State::Setup : State::Load;
            break;
        }
        case State::Exit:
            return;
        }
    }
}
