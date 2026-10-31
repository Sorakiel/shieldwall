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
        frames.push_back(f);
    }
    void showArmies(const BattleSnapshot&) override {}
    void message(const std::string&) override {}
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
void testBackAndGenerated(const UnitCatalog& catalog) {
    Fixture file;
    FlowView view;
    view.numbers = {50, 0, 0, std::nullopt, 75, 0};
    view.actions = {1,
                    uiAction::Back,
                    2,
                    uiAction::Back,
                    1,
                    1,
                    uiAction::Back,
                    uiAction::Back,
                    uiAction::Back,
                    2,
                    2,
                    2,
                    1,
                    uiAction::Back,
                    30,
                    uiAction::Back,
                    30,
                    1,
                    1,
                    2,
                    uiAction::Back,
                    3,
                    30,
                    uiAction::Back,
                    30,
                    0,
                    0};
    ConsoleUI ui(view);
    Menu(ui, catalog, file.autosave()).run();
    CHECK(view.actions.empty() && view.numbers.empty());
    CHECK((view.defaults == std::vector<std::uint32_t>{120, 42, 0, 0, 50, 0}));
    CHECK(view.cancels[0] == NumberCancel::MainMenu && view.cancels[1] == NumberCancel::Previous);
    bool blueAfterRed = false, previousWasRed = false, redGenerated = false;
    int redMethods = 0, generated = 0;
    for (const auto& f : view.frames) {
        CHECK(f.armies[1 - f.activeTeam].units.empty()); // private recruitment
        if (previousWasRed && f.activeTeam == 0) {
            CHECK(f.screen == RecruitmentScreen::Methods);
            blueAfterRed = true;
        }
        previousWasRed = f.activeTeam == 1;
        if (f.activeTeam == 1 && f.screen == RecruitmentScreen::Methods)
            ++redMethods;
        if (f.screen == RecruitmentScreen::Generated) {
            ++generated;
            redGenerated = redGenerated || f.activeTeam == 1;
        }
    }
    CHECK(blueAfterRed && redGenerated && redMethods >= 3 && generated >= 3);
    const auto data = SaveService::loadFromFile(file.autosave());
    CHECK(data.costLimit == 75 && data.seed == 0 && data.phase == SavePhase::Battle);
    CHECK(data.armyA.size() == 1 && !data.armyB.empty());
    CHECK(!view.handoffs.empty());
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
        testBackAndGenerated(catalog);
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
