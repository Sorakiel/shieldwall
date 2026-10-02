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
    std::vector<std::string> messages;
    std::vector<int> progress;
    std::vector<BattleMode> modes;
    std::function<void()> beforeText, afterText, beforeChoice;
    bool textWasRead = false;
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
        menus.push_back(choices);
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
    void message(const std::string& text) override { messages.push_back(text); }
    void showArmies(const BattleSnapshot&) override {}
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
    view.actions = {1, mode, 1, 1, 1};
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
    for (const auto mode : {BattleMode::Manual, BattleMode::Automatic}) {
        SaveDirectory dir;
        const auto initial = battle(catalog, 2);
        SaveService::saveToFile(initial, dir.autosave());
        ScriptView view;
        view.actions = {2, 1, mode == BattleMode::Manual ? 2u : 1u, 0};
        view.turns = {TurnAction::Save, TurnAction::Exit};
        view.texts = {std::string("тест")};
        observeAutosave(view, dir);
        run(view, catalog, dir);
        CHECK(fs::exists(dir.named("тест")));
        const auto saved = SaveService::loadFromFile(dir.named("тест").string());
        CHECK(saved.phase == SavePhase::Battle && saved.turn == 2);
        CHECK(view.progress == std::vector<int>{2});
        CHECK(view.modes == std::vector<BattleMode>{mode});
        std::ostringstream a, b;
        SaveService::write(initial, a); SaveService::write(saved, b);
        CHECK(a.str() == b.str());
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

void testReadySaveDefaultRetryAndCancel(const UnitCatalog& catalog) {
    {
        SaveDirectory dir; ScriptView view; ready(view);
        view.actions.insert(view.actions.end(), {40, 0});
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
        view.actions.insert(view.actions.end(), {40, 0});
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
        view.actions.insert(view.actions.end(), {40, confirm, 0});
        view.texts = {std::string("old")};
        observeAutosave(view, dir);
        run(view, catalog, dir);
        CHECK(view.hasChoice("Перезаписать"));
        CHECK((contents(dir.named("old")) == original) == (confirm == 0));
        CHECK(SaveService::loadFromFile(dir.named("old").string()).seed == (confirm ? 42u : 17u));
    }
}

void testRecruitmentAndResultSave(const UnitCatalog& catalog) {
    {
        SaveDirectory dir; ScriptView view;
        view.numbers = {70, 42};
        view.actions = {1, 2, 1, 2, 1, 40, 0, 0};
        view.texts = {std::string("draft")};
        observeAutosave(view, dir);
        run(view, catalog, dir);
        const auto saved = SaveService::loadFromFile(dir.named("draft").string());
        CHECK(saved.phase == SavePhase::Recruitment && saved.turn == 0);
        CHECK(saved.armyA.size() == 1 && saved.armyB.empty());
    }
    {
        SaveDirectory dir; ScriptView view; ready(view, 1);
        view.actions.insert(view.actions.end(), {1, 40, 0});
        view.texts = {std::string("result")};
        observeAutosave(view, dir);
        run(view, catalog, dir);
        const auto saved = SaveService::loadFromFile(dir.named("result").string());
        const auto autosave = SaveService::loadFromFile(dir.autosave());
        CHECK(saved.phase == SavePhase::Result && saved.turn > 0);
        CHECK(saved.turn == autosave.turn);
        CHECK(BattleReplay::replay(saved).engine->finished());
        CHECK(saved.armyA.size() == autosave.armyA.size());
        CHECK(saved.armyA.front().spec.maxHp == autosave.armyA.front().spec.maxHp);
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
        ScriptView view; ready(view);
        view.actions.insert(view.actions.end(), {4, 2, confirm});
        if (confirm) view.actions.insert(view.actions.end(), {2, 0});
        else view.actions.insert(view.actions.end(), {0});
        view.turns = {TurnAction::Exit};
        run(view, catalog, dir);
        CHECK(view.hasChoice("Заменить текущую партию"));
        CHECK(view.hasMessage("Сыграно ходов: 2") == (confirm == 1));
        CHECK(SaveService::loadFromFile(dir.autosave()).seed == 42);
    }
    {
        SaveDirectory dir; ScriptView view; ready(view);
        view.actions.insert(view.actions.end(), {4, 0, 40, 0});
        view.texts = {std::string("still-ready")};
        observeAutosave(view, dir);
        run(view, catalog, dir);
        CHECK(SaveService::loadFromFile(dir.named("still-ready").string()).turn == 0);
    }
    {
        SaveDirectory dir; ScriptView view; ready(view, 1);
        view.actions.insert(view.actions.end(), {1, 2, 0, 40, 0});
        view.texts = {std::string("still-result")};
        observeAutosave(view, dir);
        run(view, catalog, dir);
        CHECK(SaveService::loadFromFile(dir.named("still-result").string()).phase == SavePhase::Result);
    }
    {
        SaveDirectory dir;
        auto invalid = battle(catalog); invalid.phase = SavePhase::Result;
        SaveService::saveToFile(invalid, dir.named("invalid").string());
        ScriptView view; ready(view);
        view.actions.insert(view.actions.end(), {4, 2, 1, 40, 0});
        view.texts = {std::string("after-error")};
        run(view, catalog, dir);
        CHECK(view.hasMessage("незавершённый бой"));
        CHECK(SaveService::loadFromFile(dir.named("after-error").string()).phase == SavePhase::Battle);
    }
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
    const auto catalog = UnitCatalog::loadFromFile("data/units.txt");
    testNamedSaveDuringBattle(catalog);
    testRunnerWithoutSaveCallback(catalog);
    testReadySaveDefaultRetryAndCancel(catalog);
    testOverwrite(catalog);
    testRecruitmentAndResultSave(catalog);
    testLoadSecondAndDamaged(catalog);
    testPaging(catalog);
    testReplacingAndCancellingParty(catalog);
    testUtf8InputAndConsole();
    std::cout << (failures == 0 ? "All save UI tests passed\n" : "Save UI tests failed\n");
    return failures == 0 ? 0 : 1;
}
