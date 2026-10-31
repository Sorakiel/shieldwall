#include "BattlePlayback.h"
#include "BattleLog.h"
#include <iostream>
#include <limits>
#include <cmath>
namespace {
int failures = 0;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        if (!(c)) {                                                                                          \
            ++failures;                                                                                      \
            std::cerr << "FAIL line " << __LINE__ << ": " << #c << '\n';                                     \
        }                                                                                                    \
    } while (false)
BattleSnapshot frame() {
    BattleSnapshot s;
    s.armies[0] = {"Синие",
                   100,
                   {{UnitKind::Heavy, "Брон", 30, 45, 30, 9, 5},
                    {UnitKind::Light, "Мирн", 10, 20, 10, 7, 1},
                    {UnitKind::Archer, "Дарр", 16, 16, 18, 4, 0, 11},
                    {UnitKind::Mage, "Зэн", 14, 14, 25, 2, 0},
                    {UnitKind::Healer, "Тиль", 18, 18, 22, 2, 0}}};
    s.armies[1] = {
        "Красные", 50, {{UnitKind::Light, "Вейн", 20, 20, 10}, {UnitKind::Archer, "Орн", 16, 16, 18}}};
    return s;
}
void testTimingAndJournal() {
    for (const auto type : {EventType::Melee, EventType::Shot, EventType::Heal, EventType::Clone,
                            EventType::CloneFailed, EventType::Buff, EventType::Death, EventType::Promote,
                            EventType::BattleEnd, EventType::Miss, EventType::OutOfRange}) {
        CHECK(presentation::duration(type) >= .4f && presentation::duration(type) <= 1.2f);
        presentation::Step step;
        step.event.type = type;
        CHECK(!step.advance(-1));
        CHECK(!step.advance(std::numeric_limits<float>::quiet_NaN()));
        CHECK(step.elapsed == 0);
        CHECK(!step.advance(presentation::duration(type) * .49f));
        CHECK(!step.committed);
        CHECK(step.advance(presentation::duration(type) * .02f));
        CHECK(step.committed);
        CHECK(!step.advance(100));
        CHECK(step.finished());
        CHECK(!step.advance(1));
        CHECK(step.progress() == 1.f);
    }
    auto s = frame();
    presentation::Playback playback;
    battletext::Log log;
    BattleEvent e{EventType::Melee, 1, 0, "Брон", "Вейн", 7, 13};
    auto step = playback.begin(s, e);
    auto tick = [&](float dt) {
        if (step.advance(dt)) {
            playback.commit(s, step);
            log.append(
                U"Попадание", 0, [](const std::u32string& r) { return static_cast<float>(r.size()); }, 80);
        }
    };
    tick(.3f);
    CHECK(s.armies[1].units[0].hp == 20);
    CHECK(log.lines.empty());
    tick(.1f);
    CHECK(s.armies[1].units[0].hp == 13);
    CHECK(log.lines.size() == 1);
    tick(5);
    CHECK(log.lines.size() == 1);
    e.type = EventType::Heal;
    e.team = 0;
    e.actor = "Тиль";
    e.target = "Брон";
    e.restored = 8;
    step = playback.begin(s, e);
    CHECK(step.actor.index == 4 && step.target.index == 0);
    CHECK(!step.advance(.3f));
    CHECK(s.armies[0].units[0].hp == 30);
    CHECK(step.advance(.3f));
    playback.commit(s, step);
    CHECK(s.armies[0].units[0].hp == 38);
}
void testEffectsChangeOnlySnapshots() {
    auto s = frame();
    presentation::Playback playback;
    BattleEvent clone{EventType::Clone, 1, 0, "Зэн", "Дарр"};
    auto step = playback.begin(s, clone);
    CHECK(step.actor.index == 3 && step.target.index == 2);
    CHECK(step.subjectKind == UnitKind::Archer);
    CHECK(s.armies[0].units.size() == 5);
    playback.commit(s, step);
    CHECK(s.armies[0].units.size() == 6);
    CHECK(s.armies[0].units[3].kind == UnitKind::Archer && s.armies[0].units[3].name == "Дарр [Клон 1]");
    CHECK(s.armies[0].units[3].hp == 16);
    CHECK(s.armies[0].units[3].attack == 4 && s.armies[0].units[3].rangedAttack == 11);
    CHECK(s.armies[0].units[4].kind == UnitKind::Mage);
    BattleEvent buff{EventType::Buff, 2, 0, "Мирн", "Брон"};
    buff.buff = "щит";
    step = playback.begin(s, buff);
    playback.commit(s, step);
    CHECK(s.armies[0].units[0].name == "Брон [щит]");
    BattleEvent failed{EventType::CloneFailed, 2, 0, "Зэн", "Мирн"};
    step = playback.begin(s, failed);
    playback.commit(s, step);
    CHECK(s.armies[0].units.size() == 6);
    BattleEvent death{EventType::Death, 2, 1, "Вейн"};
    step = playback.begin(s, death);
    playback.commit(s, step);
    CHECK(s.armies[1].units[0].hp == 0);
    BattleEvent promote{EventType::Promote, 2, 1, "Орн"};
    step = playback.begin(s, promote);
    CHECK(step.fromIndex == 1);
    CHECK(s.armies[1].units.size() == 1 && s.armies[1].units[0].name == "Орн");
    CHECK(step.actor.index == 0);
    BattleEvent draw{EventType::BattleEnd, 2, -2};
    step = playback.begin(s, draw);
    CHECK(!step.actor.valid());
    CHECK(!step.target.valid());
    playback.commit(s, step);
    BattleEvent missing{EventType::Heal, 3, 0, "Неизвестен", "Неизвестен"};
    missing.restored = 99;
    step = playback.begin(s, missing);
    CHECK(!step.actor.valid() && !step.target.valid());
    playback.commit(s, step);
}
void testEquipmentStatsAtImpact() {
    auto s = frame();
    auto& heavy = s.armies[0].units[0];
    heavy.attack = 17;
    heavy.defense = 12;
    std::unique_ptr<Unit> expected = std::make_unique<HeavyUnit>("Брон", 45, 17, 12, 30);
    presentation::Playback playback;
    int turn = 0;
    for (const auto buff : {BuffKind::Helmet, BuffKind::Shield, BuffKind::Spear, BuffKind::Horse}) {
        BattleEvent event{EventType::Buff, ++turn, 0, "Мирн", heavy.name};
        event.buff = buffName(buff);
        auto step = playback.begin(s, event);
        const int beforeAttack = heavy.attack, beforeDefense = heavy.defense;
        CHECK(!step.advance(presentation::duration(event.type) * .49f));
        CHECK(heavy.attack == beforeAttack && heavy.defense == beforeDefense);
        CHECK(step.advance(presentation::duration(event.type) * .02f));
        playback.commit(s, step);
        expected = decorate(std::move(expected), buff);
        CHECK(heavy.attack == expected->meleeAttack() && heavy.defense == expected->defense());
        CHECK(heavy.name == expected->name());
        CHECK(heavy.hp == 30 && heavy.maxHp == 45 && heavy.cost == 30);
        CHECK(!step.advance(10));
        // A repeated equipment marker cannot add the same bonus twice.
        presentation::applyEquipment(heavy, event.buff);
        CHECK(heavy.attack == expected->meleeAttack() && heavy.defense == expected->defense());
    }
    CHECK(heavy.attack == 23 && heavy.defense == 18);
}
void testTeamCloneNumbers() {
    auto s = frame();
    s.armies[1].units.push_back({UnitKind::Mage, "Орн-маг", 14, 14, 25, 2, 0});
    presentation::Playback playback;
    auto first = playback.begin(s, {EventType::Clone, 1, 0, "Зэн", "Дарр"});
    playback.commit(s, first);
    CHECK(s.armies[0].units[3].cloneNumber == 1);
    CHECK(s.armies[0].units[2].cloneNumber == 0);
    auto second = playback.begin(s, {EventType::Clone, 2, 0, "Зэн", "Дарр"});
    second.target.index = 3; // clone an existing clone
    playback.commit(s, second);
    CHECK(s.armies[0].units[3].cloneNumber == 1 && s.armies[0].units[4].cloneNumber == 2);
    playback.commit(s, playback.begin(s, {EventType::Clone, 2, 1, "Орн-маг", "Орн"}));
    CHECK(s.armies[1].units[2].cloneNumber == 1);
    CHECK(s.armies[0].clonesCreated == 2 && s.armies[1].clonesCreated == 1);
    playback.commit(s, playback.begin(s, {EventType::CloneFailed, 3, 0, "Зэн", "Дарр"}));
    CHECK(s.armies[0].clonesCreated == 2);
    s.armies[0].units[4].hp = 0;
    playback.begin(s, {EventType::Promote, 3, 0, "Брон"});
    playback.commit(s, playback.begin(s, {EventType::Clone, 4, 0, "Зэн", "Дарр"}));
    CHECK(s.armies[0].units[4].cloneNumber == 3);
    CHECK(s.armies[0].clonesCreated == 3);
    CHECK(s.armies[0].units[3].cloneNumber == 1);
}
void testDuplicateNamesAndReplayBoundary() {
    auto s = frame();
    presentation::Playback playback;
    s.armies[0].units[3] = {UnitKind::Archer, "Дарр", 16, 16, 18, 4, 0, 11};
    BattleEvent shot{EventType::Shot, 1, 0, "Дарр", "Вейн", 4, 16};
    auto first = playback.begin(s, shot);
    playback.commit(s, first);
    auto second = playback.begin(s, shot);
    CHECK(first.actor.index == 2);
    CHECK(second.actor.index == 3);
    shot.turn = 2;
    CHECK(playback.begin(s, shot).actor.index == 2);
    s.armies[1].units.push_back({UnitKind::Light, "Вейн", 8, 20, 10});
    shot.target = "Вейн";
    shot.hpLeft = 3;
    shot.damage = 5;
    shot.turn = 3;
    auto hit = playback.begin(s, shot);
    CHECK(hit.target.index == 2);
    playback.commit(s, hit);
    CHECK(s.armies[1].units[2].hp == 3);
    shot.hpLeft = 0;
    shot.damage = 3;
    shot.turn = 4;
    hit = playback.begin(s, shot);
    playback.commit(s, hit);
    CHECK(hit.target.index == 2);
    BattleEvent death{EventType::Death, 4, 1, "Вейн"};
    CHECK(playback.begin(s, death).actor.index == hit.target.index);
    playback.reset();
    shot.turn = 1;
    CHECK(playback.begin(s, shot).actor.index == 2);
}
} // namespace
int main() {
    testTimingAndJournal();
    testEffectsChangeOnlySnapshots();
    testEquipmentStatsAtImpact();
    testTeamCloneNumbers();
    testDuplicateNamesAndReplayBoundary();
    std::cout << (failures ? "Presentation tests failed\n" : "Presentation tests passed\n");
    return failures ? 1 : 0;
}
