#include <iostream>
#include <deque>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include "ArmyGenerator.h"
#include "BattleRunner.h"
#include "EventFormatter.h"
#include "Menu.h"

namespace {

int failures = 0;

void check(bool ok, const char* expression, int line) {
    if (!ok) {
        ++failures;
        std::cerr << "FAIL (line " << line << "): " << expression << '\n';
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

void testFormatter() {
    const std::vector<BattleEvent> events = {
        {EventType::Melee, 12, 0, "Тиль", "Гурм", 37, -13},
        {EventType::Shot, 12, 1, "Вейн", "Тиль", 23, 19},
        {EventType::Miss, 12, 0, "Тиль"},
        {EventType::OutOfRange, 12, 1, "Вейн"},
        {EventType::Death, 12, 0, "Тиль"},
        {EventType::Promote, 12, 1, "Вейн"},
        {EventType::BattleEnd, 12, 1}
    };
    std::mt19937 rng(42);
    const std::string melee = EventFormatter::format(events[0], rng);
    CHECK(contains(melee, "37 урона, осталось -13 hp"));
    CHECK(contains(melee, "«Тиль»") && contains(melee, "«Гурм»"));
    const std::string shot = EventFormatter::format(events[1], rng);
    CHECK(contains(shot, "23 урона, осталось 19 hp"));
    CHECK(contains(shot, "«Вейн»") && contains(shot, "«Тиль»"));

    for (const BattleEvent& event : events) {
        std::set<std::string> phrases;
        for (std::uint32_t seed = 0; seed < 32; ++seed) {
            std::mt19937 phraseRng(seed);
            const std::string text = EventFormatter::format(event, phraseRng);
            CHECK(contains(text, "[Ход 12]"));
            CHECK(contains(text, event.team == 0 ? "[Синие]" : "[Красные]"));
            if (event.type == EventType::OutOfRange) CHECK(contains(text, "дальности"));
            if (event.type == EventType::Promote) CHECK(contains(text, "перв"));
            if (event.type == EventType::BattleEnd) CHECK(contains(text, "Побед"));
            phrases.insert(text);
        }
        CHECK(phrases.size() == 2);
    }

    std::istringstream noInput;
    std::ostringstream first, second;
    ConsoleUI firstUi(noInput, first), secondUi(noInput, second);
    std::mt19937 firstRng(4000000000u), secondRng(4000000000u);
    firstUi.showEvents(events, firstRng);
    secondUi.showEvents(events, secondRng);
    CHECK(first.str() == second.str());
    const auto before = first.str();
    firstUi.showEvents({}, firstRng);
    CHECK(first.str() == before);
}

void testInput() {
    std::istringstream in("\nwrong\n-1\n12tail\n4.2\n4294967296\n9\n101\n 42 \r\n0\n4294967295\n");
    std::ostringstream out;
    ConsoleUI ui(in, out);
    CHECK(ui.readNumber("> ", 10, 100) == 42u);
    CHECK(ui.readNumber("> ", 0, 1) == 0u);
    CHECK(ui.readNumber("> ", 0, std::numeric_limits<std::uint32_t>::max()) == 4294967295u);
    CHECK(!ui.readNumber("> ", 0, 1));
    CHECK(contains(out.str(), "Введите целое число от 10 до 100"));
    CHECK(contains(out.str(), "Ввод завершён"));
}

std::string menuOutput(const std::string& input, const UnitCatalog& catalog) {
    std::istringstream in(input);
    std::ostringstream out;
    ConsoleUI ui(in, out);
    Menu(ui, catalog).run();
    return out.str();
}

void testMenu() {
    const auto catalog = UnitCatalog::loadFromFile("data/units.txt");
    const std::string script = "9\n40\n42\n5\n1\n0\n";
    const std::string first = menuOutput(script, catalog);
    CHECK(first == menuOutput(script, catalog));
    CHECK(contains(first, "Введите целое число от 10"));
    CHECK(contains(first, "Введите целое число от 0 до 3"));
    CHECK(contains(first, "Уборка завершена"));
    CHECK(contains(first, "Итог: победили"));
    CHECK(contains(first, "Новая партия"));

    // Ни отмена, ни конец ввода не должны запускать бой.
    for (const std::string scriptBeforeBattle : {"", "10\n", "10\n42\n", "10\n42\n0\n"}) {
        CHECK(!contains(menuOutput(scriptBeforeBattle, catalog), "Уборка завершена"));
    }
    const std::string regenerated = menuOutput("40\n0\n2\n1\n0\n", catalog);
    CHECK(contains(regenerated, "Итог: победили"));
    const std::string reset = menuOutput("40\n42\n3\n10\n0\n1\n1\n10\n0\n1\n0\n", catalog);
    const auto result = reset.find("Итог: победили");
    CHECK(result != std::string::npos);
    CHECK(reset.find("Итог: победили", result + 1) != std::string::npos);
    CHECK(contains(reset, "Лимит: 10, seed: 0"));
}

void testRunnerUsesEngineAndLeavesInputAlone() {
    const auto catalog = UnitCatalog::loadFromFile("data/units.txt");
    ArmyGenerator generator(catalog);
    std::mt19937 firstArmyRng(51), secondArmyRng(51), logRng(42);
    Army a = generator.generate(70, firstArmyRng);
    Army b = generator.generate(70, firstArmyRng);
    Army expectedA = generator.generate(70, secondArmyRng);
    Army expectedB = generator.generate(70, secondArmyRng);
    BattleEngine engine(a, b, 42), expectedEngine(expectedA, expectedB, 42);
    std::istringstream input("0\n");
    std::ostringstream output;
    ConsoleUI ui(input, output);
    BattleRunner(ui).run(engine, a, b, logRng);
    expectedEngine.runToEnd();
    CHECK(engine.finished());
    CHECK(engine.winner() == expectedEngine.winner());
    CHECK(input.peek() == '0');
    std::ostringstream expectedOutput, actualOutput;
    ConsoleUI expectedUi(input, expectedOutput), actualUi(input, actualOutput);
    expectedUi.showArmies(expectedA, expectedB);
    actualUi.showArmies(a, b);
    CHECK(expectedOutput.str() == actualOutput.str());
    CHECK(contains(output.str(), "Строй пуст"));

    // Повторный запуск завершённого боя не вызывает лишний ход.
    std::ostringstream finishedOutput;
    ConsoleUI finishedUi(input, finishedOutput);
    BattleRunner(finishedUi).run(engine, a, b, logRng);
    CHECK(!contains(finishedOutput.str(), "Уборка завершена"));
}

class RecordingView final : public View {
public:
    std::vector<BattleSnapshot> frames;
    std::vector<std::string> messages;
    std::vector<ViewStage> stages;
    std::deque<std::uint32_t> numbers;
    std::deque<std::uint32_t> actions;
    bool closeAfterFrame = false;

    void setStage(ViewStage stage) override { stages.push_back(stage); }
    std::optional<std::uint32_t> readNumber(const std::string&, std::uint32_t,
                                           std::uint32_t) override {
        if (numbers.empty()) return std::nullopt;
        const auto number = numbers.front();
        numbers.pop_front();
        return number;
    }
    std::optional<std::uint32_t> choose(const std::vector<MenuChoice>&) override {
        if (actions.empty()) return std::nullopt;
        const auto action = actions.front();
        actions.pop_front();
        return action;
    }
    void message(const std::string& text) override { messages.push_back(text); }
    void showArmies(const BattleSnapshot& frame) override {
        frames.push_back(frame);
        if (closeAfterFrame) throw ViewClosed{};
    }
};

void testViewOwnsSnapshotsAndReceivesFormattedEvents() {
    RecordingView view;
    ConsoleUI ui(view);
    Army a, b;
    a.add(std::make_unique<LightUnit>("Тиль", 20, 7, 1, 10));
    b.add(std::make_unique<HeavyUnit>("Гурм", 45, 9, 5, 30));
    ui.showArmies(a, b);
    a.front()->takeDamage(100);
    a.removeDead();
    ui.showArmies(a, b);
    CHECK(view.frames.size() == 2);
    CHECK(view.frames[0].armies[0].units[0].name == "Тиль");
    CHECK(view.frames[0].armies[0].units[0].hp == 20);
    CHECK(view.frames[0].armies[0].units[0].maxHp == 20);
    CHECK(!view.frames[0].finished);
    CHECK(view.frames[1].finished);
    CHECK(view.frames[1].armies[0].units.empty());

    const BattleEvent event{EventType::Shot, 1, 0, "Вейн", "Гурм", 5, 40};
    std::mt19937 rng(42), expectedRng(42);
    const auto expected = EventFormatter::format(event, expectedRng);
    ui.showEvents({event}, rng);
    CHECK(view.messages.back() == expected);
    CHECK(rng() == expectedRng());
}

void testClosingViewStopsRunnerAtTurnBoundary() {
    const auto catalog = UnitCatalog::loadFromFile("data/units.txt");
    ArmyGenerator generator(catalog);
    std::mt19937 armyRng(51), expectedRng(51), logRng(42);
    Army a = generator.generate(70, armyRng), b = generator.generate(70, armyRng);
    Army expectedA = generator.generate(70, expectedRng);
    Army expectedB = generator.generate(70, expectedRng);
    BattleEngine engine(a, b, 42), expectedEngine(expectedA, expectedB, 42);
    RecordingView view;
    view.closeAfterFrame = true;
    ConsoleUI ui(view);
    bool closed = false;
    try { BattleRunner(ui).run(engine, a, b, logRng); }
    catch (const ViewClosed&) { closed = true; }
    CHECK(closed);
    CHECK(view.frames.size() == 1);
    expectedEngine.nextTurn();
    std::istringstream in;
    std::ostringstream actual, expected;
    ConsoleUI actualUi(in, actual), expectedUi(in, expected);
    actualUi.showArmies(a, b);
    expectedUi.showArmies(expectedA, expectedB);
    CHECK(actual.str() == expected.str());
}

void testMenuSendsExplicitStagesToView() {
    RecordingView view;
    view.numbers = {40, 42};
    view.actions = {1, 0};
    ConsoleUI ui(view);
    const auto catalog = UnitCatalog::loadFromFile("data/units.txt");
    Menu(ui, catalog).run();
    CHECK((view.stages == std::vector<ViewStage>{ViewStage::Setup, ViewStage::Recruitment,
                                               ViewStage::Battle, ViewStage::Result}));
    CHECK(view.frames.size() >= 2);
    CHECK(!view.frames.front().finished);
    CHECK(view.frames.back().finished);
}

}  // namespace

int main() {
    testFormatter();
    testInput();
    testMenu();
    testRunnerUsesEngineAndLeavesInputAlone();
    testViewOwnsSnapshotsAndReceivesFormattedEvents();
    testClosingViewStopsRunnerAtTurnBoundary();
    testMenuSendsExplicitStagesToView();
    std::cout << (failures == 0 ? "All UI tests passed\n" : "UI tests failed\n");
    return failures == 0 ? 0 : 1;
}
