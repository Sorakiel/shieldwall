#include <deque>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include "ConsoleUI.h"
#include "Menu.h"
#include "BattleReplay.h"
#include "SaveSlots.h"
#include "ArmyUi.h"
#include "ArmyFactory.h"
#include "ArmyBuilder.h"
namespace {
int failures = 0;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        if (!(c)) {                                                                                          \
            ++failures;                                                                                      \
            std::cerr << "FAIL " << __LINE__ << ": " << #c << '\n';                                          \
        }                                                                                                    \
    } while (false)
struct Fixture {
    std::filesystem::path dir;
    Fixture() {
        static int n = 0;
        dir = std::filesystem::path("build/flow-tests") / std::to_string(++n);
        std::filesystem::create_directories(dir);
    }
    std::string autosave() const {
        return (dir / "autosave.txt").string();
    }
    ~Fixture() {
        std::error_code e;
        for (const auto& f : std::filesystem::directory_iterator(dir, e))
            std::filesystem::remove(f.path(), e);
        std::filesystem::remove(dir, e);
    }
};
class FlowView final : public View {
public:
    std::deque<std::optional<std::uint32_t>> numbers;
    std::deque<std::uint32_t> actions;
    std::deque<ArmyEdit> edits;
    std::deque<bool> handoffResults;
    std::deque<std::string> texts;
    std::vector<std::uint32_t> defaults;
    std::vector<NumberCancel> cancels;
    std::vector<RecruitmentSnapshot> frames;
    std::vector<BattleMode> modes;
    std::vector<int> handoffs;
    std::vector<std::vector<MenuChoice>> menus;
    RecruitmentSnapshot current;
    bool recruiting = false;
    std::vector<std::vector<MenuChoice>> draftMenus;
    std::vector<std::string> messages;
    PlaybackHandler handler;
    std::function<void(const BattleEvent&)> onEvent;
    void setPlaybackHandler(PlaybackHandler h) override {
        handler = std::move(h);
    }
    void setBattleProgress(int, BattleMode m) override {
        modes.push_back(m);
    }
    std::optional<ArmyEdit> takeArmyEdit() override {
        CHECK(!edits.empty());
        if (edits.empty())
            return std::nullopt;
        auto e = edits.front();
        edits.pop_front();
        return e;
    }
    std::optional<std::uint32_t> readNumber(const std::string&, std::uint32_t, std::uint32_t) override {
        CHECK(!numbers.empty());
        if (numbers.empty())
            return std::nullopt;
        auto value = numbers.front();
        numbers.pop_front();
        if (!value)
            throw ViewBack{};
        return value;
    }
    std::optional<std::uint32_t> readSetupNumber(const std::string& p, std::uint32_t a, std::uint32_t b,
                                                 NumberCancel c, std::uint32_t initial) override {
        defaults.push_back(initial);
        cancels.push_back(c);
        return readNumber(p, a, b);
    }
    std::optional<std::string> readText(const std::string&, std::size_t) override {
        CHECK(!texts.empty());
        if (texts.empty())
            return std::nullopt;
        auto text = texts.front();
        texts.pop_front();
        return text;
    }
    std::optional<std::uint32_t> choose(const std::vector<MenuChoice>& choices) override {
        menus.push_back(choices);
        if (recruiting)
            draftMenus.push_back(choices);
        CHECK(!actions.empty());
        if (actions.empty())
            return std::nullopt;
        const auto a = actions.front();
        actions.pop_front();
        CHECK(a == uiAction::ArmyEdit ||
              std::any_of(choices.begin(), choices.end(),
                          [&](const MenuChoice& c) { return c.value == a && c.enabled; }));
        return a;
    }
    void showRecruitment(const RecruitmentSnapshot& f) override {
        current = f;
        recruiting = true;
        frames.push_back(f);
    }
    void showArmies(const BattleSnapshot&) override {
        recruiting = false;
    }
    void message(const std::string& text) override {
        messages.push_back(text);
    }
    bool handoff(int team) override {
        handoffs.push_back(team);
        if (!handoffResults.empty()) {
            const bool next = handoffResults.front();
            handoffResults.pop_front();
            if (!next)
                throw ViewBack{};
        }
        return true;
    }
    TurnAction waitForTurn(BattleMode) override {
        return TurnAction::Next;
    }
    void battleEvent(const BattleEvent& e, const std::string& text) override {
        if (onEvent)
            onEvent(e);
        View::battleEvent(e, text);
    }
};
bool enabled(const std::vector<MenuChoice>& choices, std::uint32_t action) {
    const auto found =
        std::find_if(choices.begin(), choices.end(), [&](const MenuChoice& c) { return c.value == action; });
    return found != choices.end() && found->enabled;
}
void testSetupBack(const UnitCatalog& catalog) {
    Fixture file;
    FlowView view;
    view.numbers = {50, 0, 0, std::nullopt, 75, 0};
    view.actions = {1, uiAction::Back, 2, uiAction::Back, uiAction::Back, 2, 2, 0, 0};
    ConsoleUI ui(view);
    Menu(ui, catalog, file.autosave()).run();
    CHECK(view.actions.empty() && view.numbers.empty());
    CHECK((view.defaults == std::vector<std::uint32_t>{120, 42, 0, 0, 50, 0}));
    CHECK(std::all_of(view.cancels.begin(), view.cancels.end(),
                      [](NumberCancel c) { return c == NumberCancel::Previous; }));
    for (const auto& frame : view.frames)
        CHECK(frame.armies[1 - frame.activeTeam].units.empty());
    const auto data = SaveService::loadFromFile(file.autosave());
    CHECK(data.costLimit == 75 && data.seed == 0 && data.phase == SavePhase::Recruitment);

    Fixture untouched;
    FlowView back;
    back.numbers = {std::nullopt};
    back.actions = {1, 0};
    ConsoleUI backUi(back);
    Menu(backUi, catalog, untouched.autosave()).run();
    CHECK(back.actions.empty() && !std::filesystem::exists(untouched.autosave()));
}
void testUnifiedRecruitment(const UnitCatalog& catalog) {
    Fixture file;
    FlowView view;
    view.numbers = {120, 0};
    view.actions = {1,
                    2,
                    1,
                    1,
                    1,
                    3,
                    uiAction::ArmyEdit,
                    uiAction::ArmyEdit,
                    uiAction::ClearArmy,
                    2,
                    1,
                    2,
                    uiAction::Back,
                    30,
                    uiAction::Back,
                    30,
                    3,
                    30,
                    uiAction::Back,
                    30,
                    0,
                    0};
    view.edits = {{ArmyEdit::Kind::Move, 2, 0}, {ArmyEdit::Kind::Remove, 1, 1}};
    ConsoleUI ui(view);
    Menu(ui, catalog, file.autosave()).run();
    CHECK(view.actions.empty() && view.edits.empty());
    CHECK(view.frames.size() == 16 && view.draftMenus.size() == view.frames.size());
    if (view.frames.size() != 16)
        return;
    for (const auto& choices : view.draftMenus)
        for (const auto& c : choices)
            CHECK(c.label != "Сохранить" && c.label != "Загрузить" && c.label != "Создать снова" &&
                  c.label != "Другой способ набора");
    for (const auto i : {0, 1, 2, 3, 4, 5, 6, 10, 11, 12, 13, 14, 15})
        CHECK(view.frames[i].screen == RecruitmentScreen::Methods);
    for (const auto i : {0, 6, 11, 13}) {
        CHECK(view.frames[i].armies[view.frames[i].activeTeam].units.empty());
        CHECK(!enabled(view.draftMenus[i], 30) && !enabled(view.draftMenus[i], uiAction::ClearArmy));
    }
    for (const auto i : {1, 2, 3, 4, 5, 8, 9, 10, 12, 14, 15})
        CHECK(enabled(view.draftMenus[i], 30) && enabled(view.draftMenus[i], uiAction::ClearArmy));
    std::mt19937 rng(0);
    for (const auto i : {1, 2, 3}) {
        auto factory = makeArmyFactory(i == 3 ? ArmyAlgorithm::Compact : ArmyAlgorithm::Random, catalog);
        Army expected = factory->createArmy(120, rng);
        const auto& actual = view.frames[i].armies[0].units;
        CHECK(actual.size() == expected.size());
        for (std::size_t j = 0; j < std::min(actual.size(), expected.size()); ++j)
            CHECK(actual[j].kind == expected.at(j)->kind() && actual[j].name == expected.at(j)->name());
    }
    CHECK(view.frames[4].armies[0].units[0].name == view.frames[3].armies[0].units[2].name);
    CHECK(view.frames[5].armies[0].units.size() + 1 == view.frames[4].armies[0].units.size());
    CHECK(view.frames[6].spent == 0 && view.frames[6].remaining == 120);
    CHECK(view.frames[10].activeTeam == 0 && view.frames[12].activeTeam == 0);
    CHECK(view.frames[12].armies[0].units.size() == 2); // red Back returns to blue methods
    const auto data = SaveService::loadFromFile(file.autosave());
    CHECK(data.phase == SavePhase::Battle && data.armyA.size() == 2 && !data.armyB.empty());
    CHECK(data.armyA[0].spec.kind == UnitKind::Light && data.armyA[1].spec.kind == UnitKind::Heavy);
    for (const auto& text : view.messages)
        CHECK(text != "Состав обновлён." && text.find("сформирован") == std::string::npos);
    int ready = 0;
    for (const auto& choices : view.menus)
        if (choices.front().label == "К бою") {
            ++ready;
            CHECK(choices.size() == 3 && choices[1].value == uiAction::Back && choices[2].value == 0);
        }
    CHECK(ready == 2);
}
void testDropTargets(const UnitCatalog& catalog) {
    const std::vector<ArmyCardSlot> slots = {{0, 119.5f}, {1, 270.5f}, {2, 421.5f}, {3, 572.5f}, {4, 723.5f}};
    for (std::size_t from = 0; from < slots.size(); ++from) {
        CHECK(armyDropIndex(from, slots[from].center, slots) == from);
        CHECK(armyDropIndex(from, slots[from].center - 75, slots) == from);
        CHECK(armyDropIndex(from, slots[from].center + 75, slots) == from);
        for (std::size_t to = 0; to < slots.size(); ++to) {
            const auto center = slots[to].center + (to > from ? 1.f : to < from ? -1.f : 0.f);
            const auto target = armyDropIndex(from, center, slots);
            CHECK(target == to);
            std::mt19937 rng(0);
            ArmyBuilder army(catalog, 100);
            for (int n = 0; n < 5; ++n)
                army.buy(UnitKind::Light, rng);
            const auto before = army.units();
            CHECK(army.move(from, *target) == BuildResult::Ok);
            CHECK(army.units()[to].name == before[from].name);
            for (std::size_t j = 0; j < 5; ++j)
                CHECK(army.units()[j].name ==
                      before[previousArmyIndex(j, {ArmyEdit::Kind::Move, from, to})].name);
        }
    }
    CHECK(armyDropIndex(0, 270.5f, slots) == 0); // right: not until the next center is crossed
    CHECK(armyDropIndex(1, 119.5f, slots) == 1); // left: the same boundary rule
    CHECK(armyDropIndex(2, 420.5f, {{8, 119.5f}, {9, 270.5f}, {10, 421.5f}}) == 9);
    CHECK(armyDropIndex(12, 724.f, slots) == 5); // after the last visible card on a previous page
    CHECK(!armyDropIndex(0, 0, {}));
}
void testHandoffBack(const UnitCatalog& catalog) {
    Fixture file;
    FlowView view;
    view.numbers = {75, 0};
    view.handoffResults = {false, true, false, true, false};
    view.actions = {1, 2, 2, 2, 2, 1, 30, 30, 3, 30, uiAction::Back, 0, 0};
    ConsoleUI ui(view);
    Menu(ui, catalog, file.autosave()).run();
    CHECK(view.actions.empty() && view.handoffResults.empty());
    CHECK((view.handoffs == std::vector<int>{0, 0, 1, 1, 1}));
    const auto saved = SaveService::loadFromFile(file.autosave());
    CHECK(saved.phase == SavePhase::Battle && saved.armyA.size() == 1 && !saved.armyB.empty());
}

void testTypedEdits(const UnitCatalog& catalog) {
    Fixture file;
    FlowView v;
    v.numbers = {120, 0};
    v.actions = {1, 2, 1, 2, 1, 2, 3, uiAction::ArmyEdit, uiAction::ArmyEdit, 30, 3, 30, 0, 0};
    v.edits = {{ArmyEdit::Kind::Move, 2, 0}, {ArmyEdit::Kind::Remove, 1, 1}};
    ConsoleUI ui(v);
    Menu(ui, catalog, file.autosave()).run();
    CHECK(v.edits.empty() && v.actions.empty());
    const auto data = SaveService::loadFromFile(file.autosave());
    CHECK(data.armyA.size() == 2 && data.phase == SavePhase::Battle);
    if (data.armyA.size() == 2)
        CHECK(data.armyA[0].spec.kind == UnitKind::Archer && data.armyA[1].spec.kind == UnitKind::Heavy);
    CHECK(previousArmyIndex(0, {ArmyEdit::Kind::Move, 2, 0}) == 2);
    CHECK(previousArmyIndex(1, {ArmyEdit::Kind::Move, 2, 0}) == 0);
    CHECK(previousArmyIndex(2, {ArmyEdit::Kind::Move, 0, 2}) == 0);
    CHECK(previousArmyIndex(1, {ArmyEdit::Kind::Remove, 1, 1}) == 2);
}
void testPlaybackSaves(const UnitCatalog& catalog, bool closeWindow = false) {
    Fixture file;
    FlowView v;
    v.numbers = {200, 42};
    v.actions = {1, 2, 1, 3, 30, 3, 30, 1, 0};
    v.texts = {"turn-zero", "turn-one"};
    int seen = 0;
    bool aborted = false;
    v.onEvent = [&](const BattleEvent& e) {
        if (e.turn <= seen)
            return;
        seen = e.turn;
        CHECK(static_cast<bool>(v.handler));
        const auto before = SaveService::loadFromFile(file.autosave());
        CHECK(before.turn == e.turn - 1);
        if (closeWindow)
            throw ViewClosed{};
        CHECK(v.handler(TurnAction::Save));
        const auto path = file.dir / (e.turn == 1 ? "turn-zero.txt" : "turn-one.txt");
        const auto saved = SaveService::loadFromFile(path.string());
        CHECK(saved.turn == e.turn - 1 && saved.phase == SavePhase::Battle);
        auto replay = BattleReplay::replay(saved);
        CHECK(replay.turn == e.turn - 1);
        CHECK(v.handler(e.turn == 1 ? TurnAction::Automatic : TurnAction::Manual));
        CHECK(v.modes.back() == (e.turn == 1 ? BattleMode::Automatic : BattleMode::Manual));
        if (e.turn == 2) {
            CHECK(!v.handler(TurnAction::Exit));
            aborted = true;
            throw PlaybackStopped{};
        }
    };
    ConsoleUI ui(v);
    try {
        Menu(ui, catalog, file.autosave()).run();
        CHECK(!closeWindow);
    } catch (const ViewClosed&) {
        CHECK(closeWindow);
    }
    CHECK(!v.handler);
    const auto latest = SaveService::loadFromFile(file.autosave());
    CHECK(latest.turn == (closeWindow ? 0 : 1));
    CHECK(closeWindow || aborted);
    if (!closeWindow) {
        const auto named = SaveService::loadFromFile((file.dir / "turn-one.txt").string());
        CHECK(named.turn == latest.turn);
        auto a = BattleReplay::replay(latest), b = BattleReplay::replay(named);
        CHECK(a.turn == 1 && b.turn == 1);
        CHECK(a.armyA->front()->hp() == b.armyA->front()->hp());
        CHECK(a.armyB->front()->hp() == b.armyB->front()->hp());
    }
}
void testRussianCounts() {
    for (const auto n : {0u, 5u, 11u, 12u, 14u, 25u, 111u})
        CHECK(unitCount(n) == std::to_string(n) + " бойцов");
    for (const auto n : {1u, 21u, 101u})
        CHECK(unitCount(n) == std::to_string(n) + " боец");
    for (const auto n : {2u, 3u, 4u, 22u, 104u})
        CHECK(unitCount(n) == std::to_string(n) + " бойца");
    CHECK(moneyCount(1) == "1 монета" && moneyCount(3) == "3 монеты" && moneyCount(11) == "11 монет");
}
} // namespace
int main() {
    try {
        const auto catalog = UnitCatalog::loadFromFile("data/units.txt");
        testSetupBack(catalog);
        testUnifiedRecruitment(catalog);
        testDropTargets(catalog);
        testHandoffBack(catalog);
        testTypedEdits(catalog);
        testPlaybackSaves(catalog);
        testPlaybackSaves(catalog, true);
        testRussianCounts();
    } catch (const std::exception& e) {
        std::cerr << "Flow test exception: " << e.what() << '\n';
        return 1;
    }
    std::cout << (failures ? "Flow tests failed\n" : "Flow tests passed\n");
    return failures ? 1 : 0;
}
