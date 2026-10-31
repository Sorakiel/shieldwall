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
                   {{UnitKind::Heavy, "Брон", 30, 45, 30},
                    {UnitKind::Light, "Мирн", 10, 20, 10},
                    {UnitKind::Archer, "Дарр", 16, 16, 18},
                    {UnitKind::Mage, "Зэн", 14, 14, 25},
                    {UnitKind::Healer, "Тиль", 18, 18, 22}}};
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
    CHECK(s.armies[0].units[3].kind == UnitKind::Archer && s.armies[0].units[3].name == "Дарр");
    CHECK(s.armies[0].units[3].hp == 16);
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
void testDuplicateNamesAndReplayBoundary() {
    auto s = frame();
    presentation::Playback playback;
    s.armies[0].units[3] = {UnitKind::Archer, "Дарр", 16, 16, 18};
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
    testDuplicateNamesAndReplayBoundary();
    std::cout << (failures ? "Presentation tests failed\n" : "Presentation tests passed\n");
    return failures ? 1 : 0;
}
