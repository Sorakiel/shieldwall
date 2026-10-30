#include <algorithm>
#include <iostream>
#include <sstream>
#include "BattleEngine.h"
#include "BattleReplay.h"
#include "EventFormatter.h"
#include "Healer.h"
#include "Mage.h"
#include "SaveService.h"
#include "UnitDecorator.h"

namespace {
int failures = 0;
#define CHECK(value) do { if (!(value)) { ++failures; \
    std::cerr << "FAIL line " << __LINE__ << ": " << #value << '\n'; } } while (false)

int count(const std::vector<BattleEvent>& events, EventType type, int team = -1) {
    return static_cast<int>(std::count_if(events.begin(), events.end(),
        [=](const BattleEvent& e) { return e.type == type && (team < 0 || e.team == team); }));
}

void addFront(Army& army) {
    army.add(std::make_unique<HeavyUnit>("Front", 1000, 1, 100, 1));
}

std::string state(Army& army) {
    std::ostringstream out;
    for (std::size_t i = 0; i < army.size(); ++i) {
        Unit* u = army.at(i);
        out << static_cast<int>(u->kind()) << ':' << u->name() << ':' << u->hp()
            << ':' << u->defense() << ':' << u->meleeAttack() << ';';
    }
    return out.str();
}

bool sameEvents(const std::vector<BattleEvent>& a, const std::vector<BattleEvent>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto& x = a[i]; const auto& y = b[i];
        if (x.type != y.type || x.turn != y.turn || x.team != y.team ||
            x.actor != y.actor || x.target != y.target || x.damage != y.damage ||
            x.hpLeft != y.hpLeft || x.restored != y.restored || x.buff != y.buff) return false;
    }
    return true;
}

void testCastingAndPositions() {
    Army a, b;
    addFront(a); addFront(b);
    a.add(std::make_unique<Archer>("Archer", 100, 1, 1, 10, 100, 1));
    a.add(std::make_unique<Mage>("Mage1", 100, 1, 100, 1, 100, 1));
    a.add(std::make_unique<Mage>("Mage2", 100, 1, 100, 3, 100, 1));
    a.add(std::make_unique<Healer>("Healer", 100, 1, 7, 10, 100, 1));
    BattleEngine engine(a, b, 42);
    const auto events = engine.nextTurn();
    CHECK(count(events, EventType::Clone) == 2);
    CHECK(count(events, EventType::Heal) == 1);
    CHECK(a.size() == 7);
    CHECK(a.at(3)->name() == "Mage1");
    CHECK(a.at(5)->name() == "Mage2");
    CHECK(a.front()->hp() == 1000);
    const auto heal = std::find_if(events.begin(), events.end(),
        [](const BattleEvent& e) { return e.type == EventType::Heal; });
    CHECK(heal != events.end() && heal->restored == 1 && heal->target == "Front");
    bool sawCast = false, sawShot = false;
    for (const auto& e : events) {
        if (e.type == EventType::Clone || e.type == EventType::Heal) {
            sawCast = true; CHECK(!sawShot);
        }
        if (e.type == EventType::Melee) CHECK(!sawCast);
        if (e.type == EventType::Shot) { sawShot = true; CHECK(sawCast); }
    }
}

void testFailedAndForbiddenCasting() {
    Army a, b;
    a.add(std::make_unique<Mage>("FrontMage", 100, 1, 100, 10, 100, 1));
    a.add(std::make_unique<Archer>("Target", 100, 1, 1, 1, 100, 1));
    a.add(std::make_unique<Mage>("FailMage", 100, 1, 0, 10, 100, 1));
    a.add(std::make_unique<Mage>("DeadMage", 1, 1, 100, 10, 0, 1));
    a.at(3)->takeDamage(1);
    a.add(std::make_unique<Healer>("DeadHealer", 1, 1, 100, 10, 0, 1));
    a.at(4)->takeDamage(1);
    addFront(b);
    BattleEngine engine(a, b, 7);
    const auto events = engine.nextTurn();
    CHECK(count(events, EventType::Clone) == 0);
    CHECK(count(events, EventType::CloneFailed) == 1);
    CHECK(count(events, EventType::Heal) == 0);

    Army c, d;
    c.add(std::make_unique<Healer>("FrontHealer", 100, 1, 50, 10, 100, 1));
    c.add(std::make_unique<HeavyUnit>("Wounded", 100, 1, 100, 1));
    c.at(1)->takeDamage(20);
    addFront(d);
    BattleEngine frontHealer(c, d, 7);
    CHECK(count(frontHealer.nextTurn(), EventType::Heal) == 0);
    CHECK(c.at(1)->hp() == 80);
}

void testBuffLimitAndReset() {
    bool sawBothTeams = false, sawLaterTurn = false;
    for (unsigned seed = 0; seed < 32; ++seed) {
        Army a, b;
        for (Army* army : { &a, &b }) {
            addFront(*army);
            army->add(std::make_unique<LightUnit>("Light1", 100, 1, 100, 1));
            army->add(std::make_unique<HeavyUnit>("Heavy", 100, 1, 100, 1));
            army->add(std::make_unique<LightUnit>("Light2", 100, 1, 100, 1));
        }
        BattleEngine engine(a, b, seed);
        for (int turn = 1; turn <= 4; ++turn) {
            const auto events = engine.nextTurn();
            const int ca = count(events, EventType::Buff, 0);
            const int cb = count(events, EventType::Buff, 1);
            CHECK(ca <= 1 && cb <= 1);
            sawBothTeams |= ca == 1 && cb == 1;
            sawLaterTurn |= turn > 1 && ca + cb > 0;
            bool sawAttack = false;
            for (const auto& e : events) {
                if (e.type == EventType::Melee) sawAttack = true;
                if (e.type == EventType::Buff) { CHECK(!sawAttack); CHECK(!e.buff.empty()); }
            }
        }
    }
    CHECK(sawBothTeams && sawLaterTurn);
}

void testSaveReplay() {
    Army a, b;
    for (Army* army : { &a, &b }) {
        addFront(*army);
        army->add(std::make_unique<LightUnit>("Light", 100, 1, 100, 1));
        army->add(std::make_unique<Mage>("Mage", 100, 1, 70, 3, 100, 1));
        army->add(std::make_unique<Healer>("Healer", 100, 1, 5, 5, 100, 1));
    }
    // Сохранение хранит исходные армии, seed и число уже сыгранных ходов.
    SaveData save;
    save.seed = 1234;
    save.armyA = SaveService::snapshot(a);
    save.armyB = SaveService::snapshot(b);
    BattleEngine engine(a, b, save.seed);
    for (int i = 0; i < 4; ++i) engine.nextTurn();
    save.turn = 4;
    auto replay = BattleReplay::replay(save);
    CHECK(state(a) == state(*replay.armyA));
    CHECK(state(b) == state(*replay.armyB));
    for (int i = 0; i < 3; ++i) {
        CHECK(sameEvents(engine.nextTurn(), replay.engine->nextTurn()));
        CHECK(state(a) == state(*replay.armyA));
        CHECK(state(b) == state(*replay.armyB));
    }
}

void testFormatting() {
    std::mt19937 rng(1);
    for (EventType type : { EventType::Clone, EventType::CloneFailed, EventType::Heal, EventType::Buff }) {
        BattleEvent event{ type, 2, 0, "Actor", "Target" };
        event.restored = 7;
        event.buff = "Shield";
        const auto text = EventFormatter::format(event, rng);
        CHECK(text.find("Actor") != std::string::npos && text.find("Target") != std::string::npos);
        if (type == EventType::Heal) CHECK(text.find("7 hp") != std::string::npos);
        if (type == EventType::Buff) CHECK(text.find("Shield") != std::string::npos);
    }
}
}

int main() {
    testCastingAndPositions();
    testFailedAndForbiddenCasting();
    testBuffLimitAndReset();
    testSaveReplay();
    testFormatting();
    if (failures) return 1;
    std::cout << "Ability engine tests passed\n";
}
