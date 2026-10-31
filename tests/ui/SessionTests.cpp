#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include "ArmyBuilder.h"
#include "ArmyGenerator.h"
#include "ArmyFactory.h"
#include "BattleReplay.h"
#include "BattleRunner.h"
#include "ConsoleUI.h"
#include "ConsoleView.h"
#include "Menu.h"

namespace {

int failures = 0;
#define CHECK(expression) do { if (!(expression)) { ++failures; \
    std::cerr << "FAIL line " << __LINE__ << ": " << #expression << '\n'; } } while (false)

bool contains(const std::string& value, const std::string& part) {
    return value.find(part) != std::string::npos;
}

struct SaveFile {
    std::string path;
    SaveFile() {
        static int counter = 0;
        path = "build/ui-session-tests/" + std::to_string(++counter) + "/autosave.txt";
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    }
    ~SaveFile() {
        std::error_code error;
        std::filesystem::remove(path, error);
        std::filesystem::remove(std::filesystem::path(path).parent_path(), error);
    }
};

class ScriptView final : public View {
public:
    std::deque<std::uint32_t> numbers, actions;
    std::deque<TurnAction> turns;
    std::vector<BattleSnapshot> frames;
    std::vector<std::vector<MenuChoice>> menus;
    std::vector<RecruitmentSnapshot> drafts;
    std::vector<std::string> messages;
    std::vector<int> handoffs, progress;
    std::size_t closeAtFrame = 0;
    std::function<void()> beforeTurn;
    void setBattleProgress(int turn, BattleMode) override { progress.push_back(turn); }
    std::optional<std::uint32_t> readNumber(const std::string&, std::uint32_t, std::uint32_t) override {
        if (numbers.empty()) return std::nullopt;
        const auto value = numbers.front(); numbers.pop_front(); return value;
    }
    std::optional<std::uint32_t> choose(const std::vector<MenuChoice>& choices) override {
        menus.push_back(choices);
        if (actions.empty()) return std::nullopt;
        const auto value = actions.front(); actions.pop_front(); return value;
    }
    void message(const std::string& text) override { messages.push_back(text); }
    void showArmies(const BattleSnapshot& frame) override {
        frames.push_back(frame);
        if (closeAtFrame != 0 && frames.size() == closeAtFrame) throw ViewClosed{};
    }
    void showRecruitment(const RecruitmentSnapshot& frame) override { drafts.push_back(frame); }
    bool handoff(int team) override { handoffs.push_back(team); return true; }
    TurnAction waitForTurn(BattleMode mode) override {
        if (beforeTurn) beforeTurn();
        if (turns.empty()) return mode == BattleMode::Automatic ? TurnAction::Next : TurnAction::Exit;
        const auto action = turns.front(); turns.pop_front(); return action;
    }
    bool hasMessage(const std::string& text) const {
        for (const auto& value : messages) if (contains(value, text)) return true;
        return false;
    }
};

std::string recordsText(const std::vector<UnitRecord>& records) {
    std::ostringstream text;
    for (const auto& record : records) text << dataformat::kindToken(record.spec.kind) << ':'
        << record.name << ':' << record.spec.maxHp << ':' << record.spec.cost << '\n';
    return text.str();
}

std::string armyText(Army& army) {
    std::ostringstream text;
    for (std::size_t i = 0; i < army.size(); ++i) {
        const auto& unit = *army.at(i);
        text << unit.name() << ':' << unit.hp() << ':' << unit.maxHp() << '\n';
    }
    return text.str();
}

SaveData initialBattle(const UnitCatalog& catalog) {
    std::mt19937 rng(9);
    ArmyGenerator generator(catalog);
    Army a = generator.generate(200, rng), b = generator.generate(200, rng);
    SaveData data;
    data.costLimit = 200;
    data.seed = 42;
    data.armyA = SaveService::snapshot(a);
    data.armyB = SaveService::snapshot(b);
    return data;
}

void testManualRunner(const UnitCatalog& catalog) {
    const SaveData data = initialBattle(catalog);
    auto actual = BattleReplay::replay(data), expected = BattleReplay::replay(data);
    ScriptView view;
    view.turns = {TurnAction::Automatic, TurnAction::Manual, TurnAction::Next, TurnAction::Exit};
    ConsoleUI ui(view);
    std::mt19937 rng(42);
    std::vector<int> checkpoints;
    const auto first = BattleRunner(ui).run(*actual.engine, *actual.armyA, *actual.armyB, rng,
        BattleMode::Manual, 0, [&](int turn) { checkpoints.push_back(turn); });
    expected.engine->nextTurn();
    CHECK(!first.finished && first.turn == 1 && first.mode == BattleMode::Manual);
    CHECK(checkpoints == std::vector<int>{1});
    CHECK(armyText(*actual.armyA) == armyText(*expected.armyA));
    CHECK(armyText(*actual.armyB) == armyText(*expected.armyB));
    view.turns = {TurnAction::Next, TurnAction::Exit};
    const auto second = BattleRunner(ui).run(*actual.engine, *actual.armyA, *actual.armyB, rng,
        BattleMode::Manual, first.turn, [&](int turn) { checkpoints.push_back(turn); });
    expected.engine->nextTurn();
    CHECK(second.turn == 2 && !second.finished);
    CHECK((checkpoints == std::vector<int>{1, 2}));
    CHECK(armyText(*actual.armyA) == armyText(*expected.armyA));
    CHECK(armyText(*actual.armyB) == armyText(*expected.armyB));
}

void testPurchaseAndStartingRecords(const UnitCatalog& catalog) {
    SaveFile file;
    ScriptView view;
    view.numbers = {50, 4242, 1, 2, 1};
    view.actions = {1, 2, 1, 2, 1, 2, 1, 20, 3, 1, 21, 30, 2, 3, 1, 30, 0};
    ConsoleUI ui(view);
    Menu(ui, catalog, file.path).run();
    const SaveData saved = SaveService::loadFromFile(file.path);
    CHECK(saved.phase == SavePhase::Battle && saved.turn == 0);
    CHECK(saved.costLimit == 50 && saved.seed == 4242);
    CHECK(saved.armyA.size() == 3 && saved.armyB.size() == 2);
    CHECK(view.hasMessage("Недостаточно бюджета"));
    std::mt19937 rng(4242);
    ArmyBuilder a(catalog, 50), b(catalog, 50);
    a.buy(UnitKind::Light, rng); a.buy(UnitKind::Heavy, rng); a.buy(UnitKind::Light, rng);
    a.remove(0); a.buy(UnitKind::Light, rng); a.move(1, 0);
    b.buy(UnitKind::Archer, rng); b.buy(UnitKind::Light, rng);
    CHECK(recordsText(saved.armyA) == recordsText(a.units()));
    CHECK(recordsText(saved.armyB) == recordsText(b.units()));
    bool disabled = false;
    for (const auto& draft : view.drafts) for (const auto& offer : draft.offers) {
        if (draft.remaining == 0 && !offer.enabled) disabled = true;
    }
    CHECK(disabled);
}

void testRecruitmentResume(const UnitCatalog& catalog) {
    SaveFile file;
    ScriptView first;
    first.numbers = {40, 7};
    first.actions = {1, 2, 1, 2, 1, 0, 0};
    ConsoleUI firstUi(first);
    Menu(firstUi, catalog, file.path).run();
    const SaveData partial = SaveService::loadFromFile(file.path);
    CHECK(partial.phase == SavePhase::Recruitment && partial.turn == 0);
    CHECK(partial.armyA.size() == 1 && partial.armyB.empty());
    ScriptView second;
    second.actions = {2, 1, 2, 1, 2, 2, 30, 2, 1, 30, 0};
    ConsoleUI secondUi(second);
    Menu(secondUi, catalog, file.path).run();
    const SaveData completed = SaveService::loadFromFile(file.path);
    CHECK(completed.phase == SavePhase::Battle && completed.turn == 0);
    CHECK(completed.armyA.size() == 2 && completed.armyB.size() == 1);
    CHECK(completed.armyA[0].name == partial.armyA[0].name);
    CHECK(completed.costLimit == 40 && completed.seed == 7);
}

void testBattleResumeAndResult(const UnitCatalog& catalog) {
    SaveFile file;
    SaveData original = initialBattle(catalog);
    original.turn = 3;
    SaveService::saveToFile(original, file.path);
    ScriptView view;
    view.actions = {2, 1, 2, 0};
    view.turns = {TurnAction::Next, TurnAction::Exit};
    std::vector<int> observedTurns;
    view.beforeTurn = [&] { observedTurns.push_back(SaveService::loadFromFile(file.path).turn); };
    ConsoleUI ui(view);
    Menu(ui, catalog, file.path).run();
    const SaveData continued = SaveService::loadFromFile(file.path);
    CHECK(continued.phase == SavePhase::Battle && continued.turn == 4);
    CHECK((observedTurns == std::vector<int>{3, 4}));
    CHECK(recordsText(continued.armyA) == recordsText(original.armyA));
    CHECK(recordsText(continued.armyB) == recordsText(original.armyB));
    CHECK(view.hasMessage("[Ход 4]"));
    auto expected = BattleReplay::replay(continued);
    CHECK(!view.frames.empty());
    if (!view.frames.empty()) {
        const auto& army = view.frames.back().armies[0];
        CHECK(army.units.size() == expected.armyA->size());
        for (std::size_t i = 0; i < army.units.size(); ++i) {
            CHECK(army.units[i].hp == expected.armyA->at(i)->hp());
        }
    }
    while (!expected.engine->finished()) { expected.engine->nextTurn(); ++expected.turn; }
    SaveData result = continued;
    result.phase = SavePhase::Result;
    result.turn = expected.turn;
    SaveService::saveToFile(result, file.path);
    ScriptView loadedResult;
    loadedResult.actions = {2, 1, 0};
    loadedResult.turns = {TurnAction::Next};
    ConsoleUI resultUi(loadedResult);
    Menu(resultUi, catalog, file.path).run();
    CHECK(loadedResult.turns.size() == 1);
    CHECK(loadedResult.hasMessage("Итог: победили"));
    CHECK(SaveService::loadFromFile(file.path).turn == result.turn);
}

void testBadLoadPreservesFile(const UnitCatalog& catalog) {
    SaveFile file;
    { std::ofstream out(file.path); out << "damaged-save\n"; }
    ScriptView view;
    view.actions = {2, 0, 0};
    ConsoleUI ui(view);
    Menu(ui, catalog, file.path).run();
    CHECK(view.hasMessage("Повреждён:"));
    CHECK(view.hasMessage("строка 1"));
    std::ifstream input(file.path);
    std::string line; std::getline(input, line);
    CHECK(line == "damaged-save");
    input.close();

    SaveData invalidResult = initialBattle(catalog);
    invalidResult.phase = SavePhase::Result;
    SaveService::saveToFile(invalidResult, file.path);
    ScriptView invalid;
    invalid.actions = {2, 1, 0};
    ConsoleUI invalidUi(invalid);
    Menu(invalidUi, catalog, file.path).run();
    CHECK(invalid.hasMessage("незавершённый бой"));
}

void testAutosaveFailureIsReported(const UnitCatalog& catalog) {
    SaveFile file;
    { std::ofstream out(file.path); out << "not a directory"; }
    ScriptView view;
    view.numbers = {40, 42};
    view.actions = {1,1,1,1,30,1,30,0};
    ConsoleUI ui(view);
    Menu(ui, catalog, file.path + "/autosave.txt").run();
    CHECK(view.hasMessage("Не удалось создать папку автосохранения"));
    CHECK(!view.frames.empty());
}

void testCooperativePrivacy(const UnitCatalog& catalog) {
    SaveFile file;
    ScriptView view;
    view.numbers = {40, 42};
    view.actions = {1, 1, 2, 2, 1, 30, 2, 2, 30, 0};
    ConsoleUI ui(view);
    Menu(ui, catalog, file.path).run();
    CHECK((view.handoffs == std::vector<int>{0, 1}));
    bool secondPlayer = false;
    for (const auto& frame : view.drafts) {
        CHECK(frame.armies[1 - frame.activeTeam].units.empty());
        if (frame.activeTeam == 1) secondPlayer = true;
    }
    CHECK(secondPlayer);
    const SaveData saved = SaveService::loadFromFile(file.path);
    CHECK(saved.armyA.size() == 1 && saved.armyB.size() == 1);
}

void testConsoleControls() {
    std::istringstream commands("1\n2\nwrong\n\n0\n\n");
    std::ostringstream text;
    ConsoleView view(commands, text);
    CHECK(view.choose({{1, "Недоступно", false}, {2, "Доступно"}}) == 2u);
    CHECK(view.waitForTurn(BattleMode::Manual) == TurnAction::Next);
    CHECK(view.waitForTurn(BattleMode::Automatic) == TurnAction::Next);
    CHECK(commands.peek() == '0');
    CHECK(view.waitForTurn(BattleMode::Manual) == TurnAction::Exit);
    CHECK(view.handoff(1));
    CHECK(contains(text.str(), "\033[2J"));
    CHECK(contains(text.str(), "игроку 2"));
    CHECK(contains(text.str(), "Выберите доступный пункт"));
}

void testConsoleCancellationAndNavigation(const UnitCatalog& catalog) {
    {
        std::istringstream input("/menu\n0\n 0 \n2\n/cancel\n0\n");
        std::ostringstream output;
        ConsoleUI ui(input, output);
        CHECK(!ui.readNumberCancelable("Бюджет: ", 10, 100, NumberCancel::MainMenu));
        CHECK(ui.readNumberCancelable("Seed: ", 0, 100, NumberCancel::MainMenu) == 0u);
        CHECK(!ui.readNumberCancelable("Позиция: ", 1, 3, NumberCancel::Dialog));
        CHECK(ui.readNumberCancelable("Позиция: ", 1, 3, NumberCancel::Dialog) == 2u);
        CHECK(!ui.readText("Имя: ", 64));
        CHECK(ui.readText("Имя: ", 64) == "0");
    }
    {
        std::istringstream input("/back\n0\n/back\n");std::ostringstream output;ConsoleUI ui(input,output);
        bool back=false;try{ui.readNumberCancelable("Seed: ",0,100,NumberCancel::Previous);}catch(const ViewBack&){back=true;}
        CHECK(back);CHECK(ui.readNumberCancelable("Seed: ",0,100,NumberCancel::Previous)==0u);
        back=false;try{ui.handoff(0);}catch(const ViewBack&){back=true;}CHECK(back);
    }
    for (const auto& script : {"1\n/menu\n0\n", "1\n70\n/menu\n0\n",
                               "1\n70\n0\n0\n0\n", "1\n70\n0\n2\n0\n0\n"}) {
        SaveFile file;
        std::istringstream input(script);
        std::ostringstream output;
        ConsoleUI ui(input, output);
        Menu(ui, catalog, file.path).run();
        const auto first = output.str().find("Shieldwall — новая партия");
        CHECK(first != std::string::npos);
        CHECK(output.str().find("Shieldwall — новая партия", first + 1) != std::string::npos);
        CHECK(!std::filesystem::exists(file.path));
    }
    {
        SaveFile file;
        // Cancel remove, and both the source and destination position of move.
        std::istringstream input("1\n50\n0\n2\n1\n2\n1\n1\n6\n0\n7\n0\n7\n1\n0\n9\n1\n7\n0\n0\n");
        std::ostringstream output;
        ConsoleUI ui(input, output);
        Menu(ui, catalog, file.path).run();
        const auto saved = SaveService::loadFromFile(file.path);
        CHECK(saved.phase == SavePhase::Battle && saved.seed == 0);
        CHECK(saved.armyA.size() == 2);
        CHECK(contains(output.str(), "0 — Отмена"));
        CHECK(contains(output.str(), "В главное меню"));
    }
    {
        SaveFile file;
        ScriptView view;
        view.numbers = {70, 42};
        // Ready -> red methods -> blue methods -> confirm retained armies.
        view.actions = {1,2,1,1,30,1,30,uiAction::Back,uiAction::Back,30,30,0,0};
        ConsoleUI ui(view);
        Menu(ui, catalog, file.path).run();
        CHECK(view.actions.empty());
        CHECK(SaveService::loadFromFile(file.path).phase == SavePhase::Battle);
    }
}

void testSpecialUnitRecruitment(const UnitCatalog& catalog) {
    SaveFile file;
    ScriptView first;
    first.numbers = {70, 9};
    first.actions = {1, 2, 1, 2, 4, 5, 0, 0};
    ConsoleUI firstUi(first);
    Menu(firstUi, catalog, file.path).run();
    const auto partial = SaveService::loadFromFile(file.path);
    CHECK(partial.phase == SavePhase::Recruitment);
    CHECK(partial.armyA.size() == 2 && partial.armyB.empty());
    if (partial.armyA.size() == 2) {
        CHECK(partial.armyA[0].spec.kind == UnitKind::Mage);
        CHECK(partial.armyA[0].spec.ranged == 60 && partial.armyA[0].spec.range == 3);
        CHECK(partial.armyA[1].spec.kind == UnitKind::Healer);
        CHECK(partial.armyA[1].spec.ranged == 8 && partial.armyA[1].spec.range == 3);
    }
    bool mageButton = false, healerButton = false, separateExit = true;
    for (const auto& menu : first.menus) for (const auto& choice : menu) {
        if (choice.label == "Купить: Маг") mageButton = mageButton || (choice.value != 0 && choice.enabled);
        if (choice.label == "Купить: Целитель") healerButton = healerButton || (choice.value != 0 && choice.enabled);
        if (choice.value == 0 && choice.label.find("Купить:") == 0) separateExit = false;
    }
    CHECK(mageButton && healerButton && separateExit);
    ScriptView second;
    second.actions = {2, 1, 2, 1, 30, 2, 4, 30, 0, 0};
    ConsoleUI secondUi(second);
    Menu(secondUi, catalog, file.path).run();
    const auto restored = SaveService::loadFromFile(file.path);
    CHECK(restored.phase == SavePhase::Battle && restored.turn == 0);
    CHECK(recordsText(restored.armyA) == recordsText(partial.armyA));
    CHECK(restored.armyB.size() == 1 && restored.armyB.front().spec.kind == UnitKind::Mage);

    SaveFile limited;
    ScriptView budget;
    budget.numbers = {40, 9};
    budget.actions = {1, 2, 1, 2, 4, 5, 1, 30, 1, 0, 0};
    ConsoleUI budgetUi(budget);
    Menu(budgetUi, catalog, limited.path).run();
    const auto saved = SaveService::loadFromFile(limited.path);
    CHECK(budget.hasMessage("Недостаточно бюджета"));
    CHECK(saved.armyA.size() == 2);
    std::mt19937 expectedNames(9);
    const auto mageName = ArmyGenerator::randomName(expectedNames);
    const auto lightName = ArmyGenerator::randomName(expectedNames);
    if (saved.armyA.size() == 2) {
        CHECK(saved.armyA[0].name == mageName && saved.armyA[1].name == lightName);
    }
}

void testArmyFactorySelection(const UnitCatalog& catalog) {
    for (const auto action : {1u, 3u}) {
        SaveFile file;
        ScriptView view;
        view.numbers = {200, 77};
        view.actions = {1,2,1,action,30,action,30,0,0};
        ConsoleUI ui(view);
        Menu(ui, catalog, file.path).run();
        const auto saved = SaveService::loadFromFile(file.path);
        const auto algorithm = action == 1 ? ArmyAlgorithm::Random : ArmyAlgorithm::Compact;
        const auto factory = makeArmyFactory(algorithm, catalog);
        std::mt19937 rng(77);
        Army a = factory->createArmy(200, rng), b = factory->createArmy(200, rng);
        CHECK(recordsText(saved.armyA) == recordsText(SaveService::snapshot(a)));
        CHECK(recordsText(saved.armyB) == recordsText(SaveService::snapshot(b)));
        CHECK(!view.hasMessage(factory->title()));
    }
}

void testConsoleSpecialUnits(const UnitCatalog& catalog) {
    SaveFile file;
    std::istringstream input("1\n70\n9\n2\n1\n2\n4\n5\n9\n3\n7\n0\n0\n");
    std::ostringstream output;
    ConsoleUI ui(input, output);
    Menu(ui, catalog, file.path).run();
    const auto saved = SaveService::loadFromFile(file.path);
    CHECK(saved.armyA.size() == 2 && saved.phase == SavePhase::Battle);
    CHECK(contains(output.str(), "Купить: Маг") && contains(output.str(), "Купить: Целитель"));
    CHECK(contains(output.str(), "Клонирование 60%") && contains(output.str(), "Лечение 8 HP"));
    CHECK(contains(output.str(), "Сжатый набор"));
    CHECK(!contains(output.str(), "Сохранить") && contains(output.str(), "Очистить армию"));
    // Catalog order and a smaller set of unit types must not change purchase action mapping.
    std::istringstream source("version;1\nhealer;18;2;8;3;0;22\nmage;14;2;60;3;0;25\n");
    const auto reordered = UnitCatalog::loadFromStream(source, "reordered");
    SaveFile other;
    std::istringstream commands("1\n70\n9\n2\n1\n2\n1\n2\n6\n1\n7\n0\n0\n");
    std::ostringstream text;
    ConsoleUI reorderedUi(commands, text);
    Menu(reorderedUi, reordered, other.path).run();
    const auto result = SaveService::loadFromFile(other.path);
    CHECK(result.armyA.size() == 2);
    if (result.armyA.size() == 2) {
        CHECK(result.armyA[0].spec.kind == UnitKind::Healer);
        CHECK(result.armyA[1].spec.kind == UnitKind::Mage);
    }
}

void testClosingViewKeepsCompletedTurn(const UnitCatalog& catalog) {
    SaveFile file;
    SaveData saved = initialBattle(catalog);
    SaveService::saveToFile(saved, file.path);
    ScriptView view;
    view.actions = {2, 1, 2};
    view.turns = {TurnAction::Next};
    view.closeAtFrame = 2;
    ConsoleUI ui(view);
    bool closed = false;
    try { Menu(ui, catalog, file.path).run(); }
    catch (const ViewClosed&) { closed = true; }
    CHECK(closed);
    const SaveData actual = SaveService::loadFromFile(file.path);
    CHECK(actual.phase == SavePhase::Battle && actual.turn == 1);
    CHECK(recordsText(actual.armyA) == recordsText(saved.armyA));
    CHECK(recordsText(actual.armyB) == recordsText(saved.armyB));
}

}  // namespace

int main() {
    const auto catalog = UnitCatalog::loadFromFile("data/units.txt");
    testManualRunner(catalog);
    testPurchaseAndStartingRecords(catalog);
    testRecruitmentResume(catalog);
    testBattleResumeAndResult(catalog);
    testBadLoadPreservesFile(catalog);
    testAutosaveFailureIsReported(catalog);
    testCooperativePrivacy(catalog);
    testConsoleControls();
    testSpecialUnitRecruitment(catalog);
    testArmyFactorySelection(catalog);
    testConsoleSpecialUnits(catalog);
    testConsoleCancellationAndNavigation(catalog);
    testClosingViewKeepsCompletedTurn(catalog);
    std::cout << (failures == 0 ? "All session tests passed\n" : "Session tests failed\n");
    return failures == 0 ? 0 : 1;
}
