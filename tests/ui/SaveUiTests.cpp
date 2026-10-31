#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include "ArmyGenerator.h"
#include "BattleReplay.h"
#include "BattleRunner.h"
#include "ConsoleUI.h"
#include "ConsoleView.h"
#include "Menu.h"
#include "SaveSlots.h"
#include "SaveLocation.h"
#include "TextInput.h"

namespace {
namespace fs = std::filesystem;
int failures = 0;
#define CHECK(expression) do { if (!(expression)) { ++failures; \
    std::cerr << "FAIL line " << __LINE__ << ": " << #expression << '\n'; } } while (false)

std::string contents(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}

struct SaveDirectory {
    fs::path path;
    SaveDirectory() {
        static int number = 0;
        path = fs::path("build/ui-save-tests") / std::to_string(++number);
        fs::create_directories(path);
    }
    std::string autosave() const { return (path / "autosave.txt").string(); }
    fs::path named(const std::string& name) const { return path / fs::u8path(name + ".txt"); }
    ~SaveDirectory() {
        std::error_code error;
        for (const auto& file : fs::directory_iterator(path, error)) fs::remove(file.path(), error);
        fs::remove(path, error);
    }
};

class ScriptView final : public View {
public:
    std::deque<std::uint32_t> numbers, actions;
    std::deque<std::optional<std::string>> texts;
    std::deque<TurnAction> turns;
    std::vector<std::vector<MenuChoice>> menus;
    std::vector<std::string> messages, visibleMessages;
    std::vector<int> progress;
    std::vector<BattleMode> modes;
    std::function<void()> beforeText, afterText, beforeChoice;
    ViewStage stage = ViewStage::MainMenu;
    void setStage(ViewStage value) override { stage = value; }
    bool textWasRead = false;
    bool displayedArmy = false, displayedResult = false;
    std::vector<bool> cleanModeScreens;
    void clearDisplay() override { displayedArmy = displayedResult = false; visibleMessages.clear(); }
    void showResult(int, const std::string& text) override {
        displayedResult = true;
        message(text);
    }
    std::optional<std::uint32_t> readNumber(const std::string&, std::uint32_t, std::uint32_t) override {
        if (numbers.empty()) return std::nullopt;
        const auto number = numbers.front(); numbers.pop_front(); return number;
    }
    std::optional<std::string> readText(const std::string&, std::size_t limit) override {
        CHECK(limit == 64);
        if (beforeText) beforeText();
        textWasRead = true;
        if (texts.empty()) return std::nullopt;
        const auto text = texts.front(); texts.pop_front(); return text;
    }
    std::optional<std::uint32_t> choose(const std::vector<MenuChoice>& choices) override {
        if (textWasRead && afterText) afterText();
        if (beforeChoice) beforeChoice();
        for (const auto& choice : choices) {
            if (choice.label == "Выход") CHECK(stage == ViewStage::MainMenu);
            if (stage == ViewStage::Setup || stage == ViewStage::Recruitment) {
                CHECK(choice.label != "Сохранить" && choice.label != "Загрузить" &&
                      choice.label != "Настройки" && choice.label != "Изменить армии");
            }
            if (stage == ViewStage::Result) CHECK(choice.label != "Сохранить");
        }
        menus.push_back(choices);
        if (!choices.empty() && choices.front().label == "Автобой") {
            cleanModeScreens.push_back(!displayedArmy && !displayedResult);
        }
        if (actions.empty()) return std::nullopt;
        const auto value = actions.front(); actions.pop_front();
        bool enabled = false;
        for (const auto& choice : choices) if (choice.value == value && choice.enabled) enabled = true;
        CHECK(enabled);
        return value;
    }
    void setBattleProgress(int turn, BattleMode mode) override {
        progress.push_back(turn); modes.push_back(mode);
    }
    TurnAction waitForTurn(BattleMode mode) override {
        if (textWasRead && afterText) afterText();
        if (turns.empty()) return mode == BattleMode::Automatic ? TurnAction::Next : TurnAction::Exit;
        const auto action = turns.front(); turns.pop_front(); return action;
    }
    void message(const std::string& text) override { messages.push_back(text); visibleMessages.push_back(text); }
    void showArmies(const BattleSnapshot&) override { displayedArmy = true; }
    bool hasMessage(const std::string& part) const {
        for (const auto& text : messages) if (text.find(part) != std::string::npos) return true;
        return false;
    }
    bool hasChoice(const std::string& label) const {
        for (const auto& menu : menus) for (const auto& choice : menu)
            if (choice.label == label) return true;
        return false;
    }
};

void ready(ScriptView& view, std::uint32_t mode = 2) {
    view.numbers = {70, 42};
    view.actions = {1,mode,1,1,30,1,30};
}
void run(ScriptView& view, const UnitCatalog& catalog, const SaveDirectory& dir) {
    ConsoleUI ui(view);
    Menu(ui, catalog, dir.autosave()).run();
}
SaveData battle(const UnitCatalog& catalog, int turn = 0) {
    ArmyGenerator generator(catalog);
    std::mt19937 rng(9);
    Army a = generator.generate(70, rng), b = generator.generate(70, rng);
    SaveData data;
    data.seed = 42; data.costLimit = 70; data.turn = turn;
    data.armyA = SaveService::snapshot(a); data.armyB = SaveService::snapshot(b);
    return data;
}

void observeAutosave(ScriptView& view, const SaveDirectory& dir) {
    auto before = std::make_shared<std::string>();
    auto time = std::make_shared<fs::file_time_type>();
    view.beforeText = [&, before, time] {
        *before = contents(dir.autosave()); *time = fs::last_write_time(dir.autosave());
    };
    view.afterText = [&, before, time] {
        CHECK(contents(dir.autosave()) == *before);
        CHECK(fs::last_write_time(dir.autosave()) == *time);
    };
}

void testNamedSaveDuringBattle(const UnitCatalog& catalog) {
    for (const auto* name : {u8"тест", u8"тест 日本語"}) {
        for (const auto mode : {BattleMode::Manual, BattleMode::Automatic}) {
            SaveDirectory dir;
            const auto initial = battle(catalog, 2);
            SaveService::saveToFile(initial, dir.autosave());
            ScriptView view;
            view.actions = {2, 1, mode == BattleMode::Manual ? 2u : 1u, 0};
            view.turns = {TurnAction::Save, TurnAction::Exit};
            view.texts = {std::string(name)};
            observeAutosave(view, dir);
            run(view, catalog, dir);
            CHECK(fs::exists(dir.named(name)));
            const auto saved = SaveService::loadFromFile(dir.named(name).string());
            CHECK(saved.phase == SavePhase::Battle && saved.turn == 2);
            CHECK(view.progress == std::vector<int>{2});
            CHECK(view.modes == std::vector<BattleMode>{mode});
            std::ostringstream a, b;
            SaveService::write(initial, a); SaveService::write(saved, b);
            CHECK(a.str() == b.str());
            const auto summaries = SaveSlots::list(dir.path.string());
            CHECK(summaries.size() == 2);
            if (summaries.size() == 2) {
                CHECK(summaries[1].valid && summaries[1].name == name);
                CHECK(summaries[1].path == dir.named(name).string());
            }
            const auto savedContents = contents(dir.named(name));
            ScriptView loaded;
            loaded.actions = {2, 2, 2, 0};
            loaded.turns = {TurnAction::Exit};
            run(loaded, catalog, dir);
            CHECK(loaded.hasMessage("Сыграно ходов: 2"));
            CHECK(loaded.progress == std::vector<int>{2});
            CHECK(contents(dir.named(name)) == savedContents);
        }
    }
}

void testRunnerWithoutSaveCallback(const UnitCatalog& catalog) {
    auto session = BattleReplay::replay(battle(catalog));
    const auto a = SaveService::snapshot(*session.armyA);
    ScriptView view; view.turns = {TurnAction::Save, TurnAction::Exit};
    ConsoleUI ui(view); std::mt19937 rng(42), expected(42);
    int checkpoints = 0;
    const auto result = BattleRunner(ui).run(*session.engine, *session.armyA, *session.armyB,
        rng, BattleMode::Automatic, 0, [&](int) { ++checkpoints; });
    CHECK(result.turn == 0 && result.mode == BattleMode::Automatic && !result.finished);
    CHECK(checkpoints == 0 && rng() == expected());
    CHECK(SaveService::snapshot(*session.armyA).front().name == a.front().name);
    CHECK(session.armyA->front()->hp() == a.front().spec.maxHp);
}

void testBattleSaveDefaultRetryAndCancel(const UnitCatalog& catalog) {
    {
        SaveDirectory dir; ScriptView view; ready(view);
        view.actions.insert(view.actions.end(), {1, 0});
        view.turns = {TurnAction::Save, TurnAction::Exit};
        view.texts = {std::string("../x"), std::string("AUTOSAVE"), std::string("")};
        observeAutosave(view, dir);
        run(view, catalog, dir);
        CHECK(view.hasMessage("Не сохранено:"));
        CHECK(view.texts.empty());
        CHECK(fs::exists(dir.named("save-1")));
        CHECK(!fs::exists(dir.path.parent_path() / "x.txt"));
        const auto saved = SaveService::loadFromFile(dir.named("save-1").string());
        CHECK(saved.phase == SavePhase::Battle && saved.turn == 0);
    }
    {
        SaveDirectory dir; ScriptView view; ready(view);
        view.actions.insert(view.actions.end(), {1, 0});
        view.turns = {TurnAction::Save, TurnAction::Exit};
        view.texts = {std::nullopt};
        observeAutosave(view, dir);
        run(view, catalog, dir);
        CHECK(SaveSlots::list(dir.path.string()).size() == 1);
        CHECK(!view.hasMessage("Сохранено:"));
        CHECK(view.hasChoice("К бою"));
    }
}

void testOverwrite(const UnitCatalog& catalog) {
    for (const auto confirm : {0u, 1u}) {
        SaveDirectory dir;
        auto existing = battle(catalog); existing.seed = 17;
        SaveService::saveToFile(existing, dir.named("old").string());
        const auto original = contents(dir.named("old"));
        ScriptView view; ready(view);
        view.actions.insert(view.actions.end(), {1, confirm, 0});
        view.turns = {TurnAction::Save, TurnAction::Exit};
        view.texts = {std::string("old")};
        observeAutosave(view, dir);
        run(view, catalog, dir);
        CHECK(view.hasChoice("Перезаписать"));
        CHECK((contents(dir.named("old")) == original) == (confirm == 0));
        CHECK(SaveService::loadFromFile(dir.named("old").string()).seed == (confirm ? 42u : 17u));
    }
}

void testRecruitmentAndResultAutosave(const UnitCatalog& catalog) {
    {
        SaveDirectory dir; ScriptView view;
        view.numbers = {70, 42};
        view.actions = {1, 2, 1, 2, 1, 0, 0};
        run(view, catalog, dir);
        const auto saved = SaveService::loadFromFile(dir.autosave());
        CHECK(saved.phase == SavePhase::Recruitment && saved.turn == 0);
        CHECK(saved.armyA.size() == 1 && saved.armyB.empty());
        CHECK(!view.hasChoice("Сохранить"));
    }
    {
        SaveDirectory dir; ScriptView view; ready(view, 1);
        view.actions.insert(view.actions.end(), {1, 0, 0});
        run(view, catalog, dir);
        const auto saved = SaveService::loadFromFile(dir.autosave());
        CHECK(saved.phase == SavePhase::Result && saved.turn > 0);
        CHECK(BattleReplay::replay(saved).engine->finished());
        CHECK(!view.hasChoice("Сохранить") && !view.hasChoice("Настройки") && !view.hasChoice("Изменить армии"));
    }
}

void testLoadSecondAndDamaged(const UnitCatalog& catalog) {
    SaveDirectory dir;
    SaveService::saveToFile(battle(catalog), dir.autosave());
    const auto named = battle(catalog, 2);
    SaveService::saveToFile(named, dir.named("second").string());
    { std::ofstream out(dir.named("broken"), std::ios::binary); out << "damaged\n"; }
    fs::last_write_time(dir.named("broken"), fs::file_time_type::clock::now() - std::chrono::hours(24));
    ScriptView view;
    view.actions = {2, 2, 2, 0}; view.turns = {TurnAction::Exit};
    run(view, catalog, dir);
    CHECK(view.hasMessage("Сыграно ходов: 2"));
    CHECK(view.progress == std::vector<int>{2});
    bool disabled = false, metadata = false;
    for (const auto& menu : view.menus) for (const auto& choice : menu) {
        if (choice.label.find("broken") != std::string::npos) disabled = !choice.enabled;
        if (choice.label.find("second") != std::string::npos) metadata =
            choice.label.find("Бой, ход 2 | лимит 70 | seed 42 | ") != std::string::npos;
    }
    CHECK(disabled && metadata);
    CHECK(contents(dir.named("broken")) == "damaged\n");
    // Консоль тоже не допускает выбор отключённой строки.
    std::istringstream input("3\n2\n"); std::ostringstream output;
    ConsoleView console(input, output);
    CHECK(console.choose(view.menus.at(1)) == 2u);
    CHECK(output.str().find("Выберите доступный пункт") != std::string::npos);
}

void testPaging(const UnitCatalog& catalog) {
    SaveDirectory dir;
    SaveService::saveToFile(battle(catalog), dir.autosave());
    for (int i = 1; i <= 7; ++i) SaveService::saveToFile(battle(catalog),
        dir.named("named-" + std::to_string(i)).string());
    ScriptView view; view.actions = {2, 9, 10, 9, 0, 0};
    run(view, catalog, dir);
    CHECK(view.actions.empty());
    CHECK(view.hasMessage("страница 2 из 2"));
    int pages = 0;
    for (const auto& menu : view.menus) {
        int files = 0;
        for (const auto& choice : menu) if (choice.label.find('\n') != std::string::npos) ++files;
        if (files > 0) { ++pages; CHECK(files <= 6); }
    }
    CHECK(pages == 4);
}

void testReplacingAndCancellingParty(const UnitCatalog& catalog) {
    for (const auto confirm : {0u, 1u}) {
        SaveDirectory dir;
        auto other = battle(catalog, 2); other.seed = 15;
        SaveService::saveToFile(other, dir.named("other").string());
        ScriptView view; ready(view, 1);
        view.actions.insert(view.actions.end(), {1, 2, 2, confirm});
        if (confirm) view.actions.insert(view.actions.end(), {2, 0});
        else view.actions.insert(view.actions.end(), {0, 0});
        view.beforeChoice = [&] {
            if (view.stage == ViewStage::Result) view.turns = {TurnAction::Exit};
        };
        run(view, catalog, dir);
        CHECK(view.actions.empty());
        CHECK(view.hasChoice("Заменить текущую партию"));
        CHECK(view.hasMessage("Сыграно ходов: 2") == (confirm == 1));
        CHECK(SaveService::loadFromFile(dir.autosave()).seed == 42);
    }
    {
        SaveDirectory dir; ScriptView view; ready(view, 1);
        view.actions.insert(view.actions.end(), {1, 2, 0, 0, 0});
        int results = 0;
        view.beforeChoice = [&] {
            if (view.stage == ViewStage::Result) {
                CHECK(view.displayedArmy && view.displayedResult);
                ++results;
            }
        };
        run(view, catalog, dir);
        CHECK(view.actions.empty() && results == 3);
        CHECK(SaveService::loadFromFile(dir.autosave()).phase == SavePhase::Result);
    }
    {
        SaveDirectory dir;
        auto invalid = battle(catalog); invalid.phase = SavePhase::Result;
        SaveService::saveToFile(invalid, dir.named("invalid").string());
        ScriptView view; ready(view, 1);
        view.actions.insert(view.actions.end(), {1, 2, 2, 1, 0, 0});
        run(view, catalog, dir);
        CHECK(view.actions.empty());
        CHECK(view.hasMessage("незавершённый бой"));
        CHECK(SaveService::loadFromFile(dir.autosave()).phase == SavePhase::Result);
    }
}

void testDeleteSaves(const UnitCatalog& catalog) {
    for (const bool confirm : {false, true}) {
        SaveDirectory dir;
        SaveService::saveToFile(battle(catalog), dir.named("good").string());
        std::ofstream(dir.named("broken")) << "invalid";
        const auto list = SaveSlots::list(dir.path.string());
        std::uint32_t broken = 0;
        for (std::size_t index = 0; index < list.size(); ++index) {
            if (list[index].name == "broken") broken = static_cast<std::uint32_t>(index + 1);
        }
        CHECK(broken != 0);
        ScriptView view;
        // Two saves: toggleDelete = count + 3 = 5. Damaged saves can be deleted.
        view.actions = {2, 5, broken, confirm ? 1u : 0u, 0, 0};
        run(view, catalog, dir);
        CHECK(fs::exists(dir.named("broken")) == !confirm);
        CHECK(fs::exists(dir.named("good")));
        CHECK(view.hasMessage("Сохранение удалено") == confirm);
    }
    {
        SaveDirectory dir;
        SaveService::saveToFile(battle(catalog), dir.autosave());
        ScriptView view; view.actions = {2, 4, 1, 1, 0};
        run(view, catalog, dir);
        CHECK(!fs::exists(dir.autosave()));
        CHECK(view.hasMessage("Сохранений нет"));
    }
    {
        SaveDirectory dir;
        for (int index = 0; index < 7; ++index) {
            SaveService::saveToFile(battle(catalog), dir.named("slot-" + std::to_string(index)).string());
        }
        const auto list = SaveSlots::list(dir.path.string());
        const auto last = list.back().path;
        ScriptView view;
        // Seven saves: next = 8, delete mode = 10; remove only row on final page.
        view.actions = {2, 8, 10, 7, 1, 0, 0};
        run(view, catalog, dir);
        CHECK(!fs::exists(last));
        CHECK(SaveSlots::list(dir.path.string()).size() == 6);
        CHECK(view.hasMessage("страница 1 из 1"));
    }
}

void testLoadClearsPreviousDisplay(const UnitCatalog& catalog) {
    for (const auto phase : {SavePhase::Recruitment, SavePhase::Battle}) {
        SaveDirectory dir;
        auto saved = battle(catalog);
        saved.phase = phase;
        SaveService::saveToFile(saved, dir.named("other").string());
        ScriptView view; ready(view, 1);
        // Finish current game, then load the other file and confirm replacement.
        view.actions.insert(view.actions.end(), {1, 2, 2, 1, 2});
        if (phase == SavePhase::Recruitment) {
            view.actions.insert(view.actions.end(), {1, 30, 30, 0, 0});
        } else {
            // Auto finish old battle, then exit the loaded battle immediately.
            view.actions.push_back(0);
        }
        if (phase == SavePhase::Battle) {
            view.beforeChoice = [&] {
                if (view.displayedResult) view.turns = {TurnAction::Exit};
            };
        }
        run(view, catalog, dir);
        CHECK(view.cleanModeScreens.size() == 2);
        for (const bool clean : view.cleanModeScreens) CHECK(clean);
        CHECK(view.hasMessage("Сохранение загружено"));
    }
}

void testBackFromLoadedModeRestoresResult(const UnitCatalog& catalog) {
    SaveDirectory dir;SaveService::saveToFile(battle(catalog),dir.named("candidate").string());
    ScriptView view;ready(view,1);
    view.actions.insert(view.actions.end(),{1,2,2,1,uiAction::Back,0,0,0});
    bool restored=false;
    view.beforeChoice=[&] {
        if(view.stage==ViewStage::Result && view.cleanModeScreens.size()>=2) {
            CHECK(view.displayedArmy && view.displayedResult);restored=true;
        }
    };
    run(view,catalog,dir);
    CHECK(restored && view.actions.empty());
    CHECK(SaveService::loadFromFile(dir.autosave()).phase==SavePhase::Result);
}

void testCancelLoadedSettings(const UnitCatalog& catalog) {
    for (const auto phase : {SavePhase::Recruitment, SavePhase::Battle}) {
        SaveDirectory dir;
        auto saved = battle(catalog);
        saved.phase = phase;
        SaveService::saveToFile(saved, dir.autosave());
        const auto original = contents(dir.autosave());
        ScriptView view; view.actions = {2, 1, 0, 0};
        run(view, catalog, dir);
        CHECK(view.actions.empty());
        CHECK(view.cleanModeScreens.size() == 1 && view.cleanModeScreens.front());
        CHECK(contents(dir.autosave()) == original);
        CHECK(view.stage == ViewStage::MainMenu);
    }
}

void testLoadNoticeSurvivesReturn(const UnitCatalog& catalog) {
    SaveDirectory dir;
    ScriptView view;
    view.actions = {2, 0};
    run(view, catalog, dir);
    bool visible = false;
    for (const auto& message : view.visibleMessages) if (message == "Сохранений нет.") visible = true;
    CHECK(visible);
    // Match the terminal's last clear, rather than checking erased output history.
    std::istringstream input("2\n0\n");
    std::ostringstream output;
    ConsoleUI ui(input, output);
    Menu(ui, catalog, dir.autosave()).run();
    const auto lastClear = output.str().rfind("\033[3J");
    CHECK(lastClear != std::string::npos);
    CHECK(output.str().substr(lastClear).find("Сохранений нет.") != std::string::npos);
    // An invalid result save must show its load error after returning to the menu.
    auto invalid = battle(catalog);
    invalid.phase = SavePhase::Result;
    SaveService::saveToFile(invalid, dir.named("invalid").string());
    ScriptView failed; failed.actions = {2, 1, 0};
    run(failed, catalog, dir);
    visible = false;
    for (const auto& message : failed.visibleMessages) {
        if (message.find("незавершённый бой") != std::string::npos) visible = true;
    }
    CHECK(visible);
}

void testSaveLocationAndLegacyImport(const UnitCatalog& catalog) {
    SaveDirectory fixture;
    const auto base = fs::absolute(fixture.path);
    const auto project = base / "project";
    const auto executable = project / "build" / "Release";
    const auto firstCwd = base / "first", secondCwd = base / "second";
    fs::create_directories(executable / "saves");
    fs::create_directories(firstCwd / "saves");
    fs::create_directories(secondCwd);
    fs::create_directories(project / "data");
    fs::create_directories(project / "saves");
    std::ofstream(project / "CMakeLists.txt") << "project fixture";
    std::ofstream(project / "data/units.txt") << "catalog fixture";
    const auto original = battle(catalog);
    auto current = original; current.seed = 17;
    const auto target = executable / "saves";
    SaveService::saveToFile(current, (target / "autosave.txt").string());
    const auto before = contents(target / "autosave.txt");
    SaveService::saveToFile(original, (firstCwd / "saves" / fs::u8path("тест 日本語.txt")).string());
    std::ofstream(firstCwd / "saves/broken.txt") << "broken legacy file";
    auto legacy = original; legacy.seed = 99;
    SaveService::saveToFile(legacy, (project / "saves/autosave.txt").string());
    const auto sourceBefore = contents(project / "saves/autosave.txt");
    const auto first = prepareSaveLocation(executable, firstCwd);
    CHECK(first.directory == target);
    CHECK(contents(target / "autosave.txt") == before);
    CHECK(contents(project / "saves/autosave.txt") == sourceBefore);
    CHECK(!first.messages.empty());
    const auto list = SaveSlots::list(target.string());
    CHECK(list.size() == 4);
    bool broken = false, legacyConflict = false, unicode = false;
    for (const auto& item : list) {
        if (item.name == "broken") broken = !item.valid;
        if (item.name == "legacy-1") legacyConflict = item.valid;
        if (item.name == "тест 日本語") unicode = item.valid;
    }
    CHECK(broken && legacyConflict && unicode);
    const auto again = prepareSaveLocation(executable, secondCwd);
    CHECK(again.directory == target && again.messages.empty());
    CHECK(SaveSlots::list(target.string()).size() == 4);
    // Import markers prevent deletion from being undone on the next launch.
    const auto imported = target / fs::u8path("тест 日本語.txt");
    fs::remove(imported);
    prepareSaveLocation(executable, firstCwd);
    CHECK(!fs::exists(imported));
    CHECK(fs::exists(firstCwd / "saves" / fs::u8path("тест 日本語.txt")));
    CHECK(contents(target / "autosave.txt") == before);
    // The fixture alone owns this subtree; cleanup cannot touch real user saves.
    fs::remove_all(project);
    fs::remove_all(firstCwd);
    fs::remove_all(secondCwd);
}

void testSequentialConsoleMenus() {
    std::istringstream input("3\n1\n0\n1\n2\n1\n2\n4\n");
    std::ostringstream output;
    ConsoleUI ui(input, output);
    const std::vector<MenuChoice> choices{{1, "Новая партия"}, {2, "Загрузить"},
                                         {40, "Сохранить"}, {0, "В главное меню"}};
    CHECK(ui.choose(choices) == 40u);
    CHECK(ui.choose(choices) == 1u);
    CHECK(ui.choose(choices) == 0u);
    const std::vector<MenuChoice> page{{7, "Слот 7"}, {8, "Слот 8"},
                                      {20, "Далее"}, {0, "Отмена"}};
    CHECK(ui.choose(page) == 7u);
    CHECK(ui.choose(page) == 8u);
    const std::vector<MenuChoice> disabled{{20, "Недоступно", false}, {30, "Готово"},
                                          {40, "Сохранить"}, {0, "Отмена"}};
    CHECK(ui.choose(disabled) == 30u); // Disabled position 1 is rejected without changing numbering.
    // Position 4 is invalid; EOF cancels instead of interpreting old action ID 40.
    CHECK(!ui.choose(disabled));
    CHECK(output.str().find("3 — Сохранить") != std::string::npos);
    CHECK(output.str().find("40 —") == std::string::npos);
    CHECK(output.str().find("7 — Слот") == std::string::npos);
}

void testUtf8InputAndConsole() {
    TextInput input; input.reset(5);
    CHECK(input.append(U'т') && input.append(U'е') && input.append(U'a'));
    CHECK(input.value() == "теa" && !input.append(U'с'));
    input.backspace(); input.backspace();
    CHECK(input.value() == "т");
    CHECK(!input.append(U'\n') && !input.append(0xd800) && !input.append(0x110000));
    input.reset(64);
    for (int i = 0; i < 32; ++i) CHECK(input.append(U'я'));
    CHECK(input.value().size() == 64 && !input.append(U'я'));
    input.backspace(); CHECK(input.value().size() == 62);
    input.reset(4); CHECK(input.append(0x1f600));
    input.backspace(); CHECK(input.value().empty());
    std::istringstream commands("тест\n\n" + std::string(65, 'x') + "\nok\ns\nS\n");
    std::ostringstream output; ConsoleUI ui(commands, output);
    CHECK(ui.readText("> ", 64) == "тест");
    CHECK(ui.readText("> ", 64) == "");
    CHECK(ui.readText("> ", 64) == "ok");
    CHECK(ui.waitForTurn(BattleMode::Manual) == TurnAction::Save);
    CHECK(ui.waitForTurn(BattleMode::Manual) == TurnAction::Save);
    CHECK(!ui.readText("> ", 64));
}
}  // namespace

int main() {
    try {
        const auto catalog = UnitCatalog::loadFromFile("data/units.txt");
        testNamedSaveDuringBattle(catalog);
        testRunnerWithoutSaveCallback(catalog);
        testBattleSaveDefaultRetryAndCancel(catalog);
        testOverwrite(catalog);
        testRecruitmentAndResultAutosave(catalog);
        testLoadSecondAndDamaged(catalog);
        testPaging(catalog);
        testReplacingAndCancellingParty(catalog);
        testUtf8InputAndConsole();
        testDeleteSaves(catalog);
        testLoadClearsPreviousDisplay(catalog);
        testBackFromLoadedModeRestoresResult(catalog);
        testCancelLoadedSettings(catalog);
        testLoadNoticeSurvivesReturn(catalog);
        testSaveLocationAndLegacyImport(catalog);
        testSequentialConsoleMenus();
        std::cout << (failures == 0 ? "All save UI tests passed\n" : "Save UI tests failed\n");
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Save UI test exception: " << error.what() << '\n';
        return 1;
    }
}
