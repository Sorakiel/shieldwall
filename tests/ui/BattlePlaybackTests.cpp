#include "BattlePlayback.h"
#include "BattleLog.h"
#include "BattleMotion.h"
#include "ArmyUi.h"
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
void testPlaybackRatesAndScrolling() {
    for (const auto speed : presentation::Speeds) {
        presentation::Step step;
        step.event.type = EventType::Shot;
        const auto seconds = presentation::duration(step.event.type) * 2 / speed;
        CHECK(!step.advance(presentation::playbackSeconds(seconds * .49f, speed)));
        CHECK(step.advance(presentation::playbackSeconds(seconds * .02f, speed)));
        CHECK(!step.advance(presentation::playbackSeconds(seconds, speed, true)));
        CHECK(step.advance(presentation::playbackSeconds(seconds, speed)) == false);
        CHECK(step.finished());
    }
    CHECK(presentation::playbackSeconds(2, .5f) == .5f);
    CHECK(presentation::playbackSeconds(2, 1) == 1);
    CHECK(presentation::playbackSeconds(2, 2) == 2);
    CHECK(presentation::playbackSeconds(2, 4) == 4);
    CHECK(presentation::playbackSeconds(2, 1, true) == 0);
    presentation::Scroll scroll(0, 10);
    CHECK(scroll.value() == 0);
    CHECK(!scroll.advance(scroll.duration * .4f));
    CHECK(scroll.value() > 0 && scroll.value() < 10);
    const auto middle = scroll.value();
    CHECK(!scroll.advance(presentation::playbackSeconds(1, 1, true)) && scroll.value() == middle);
    presentation::Scroll reverse(scroll.value(), 3);
    CHECK(reverse.value() == middle);
    CHECK(scroll.advance(100) && scroll.value() == 10);
    CHECK(reverse.advance(100) && reverse.value() == 3);
    CHECK(presentation::focusOffset(0, 8, 9, 10, 5) == 5);
    CHECK(presentation::focusOffset(5, 7, 9, 10, 5) == 5);
    CHECK(presentation::focusOffset(5, 2, 4, 10, 5) == 2);
}
void testContinuousArmyScrolling() {
    CHECK(presentation::dragScrollDirection(1317, 650) == 1);
    CHECK(presentation::dragScrollDirection(117, 650) == -1);
    CHECK(presentation::dragScrollDirection(1317, 450) == 0);
    CHECK(presentation::wheelOffset(0, -1, 30, 9) == 2);
    CHECK(presentation::wheelOffset(2, -3, 30, 9) == 8);
    CHECK(presentation::wheelOffset(8, .5f, 30, 9) == 7);
    CHECK(presentation::wheelOffset(19, -5, 30, 9) == 21);
    CHECK(presentation::wheelOffset(3, 5, 30, 9) == 0);
    CHECK(presentation::wheelOffset(0, -10, 8, 9) == 0);
    float at = 0;
    for (int frame = 0; frame < 60; ++frame) at = presentation::edgeScrollOffset(at, 1, 1.f/60, 30, 9);
    CHECK(std::abs(at - 2) < .001f);
    CHECK(presentation::edgeScrollOffset(21, 1, .5f, 30, 9) == 21);
    CHECK(presentation::edgeScrollOffset(0, -1, .5f, 30, 9) == 0);
    CHECK(presentation::edgeScrollOffset(7.25f, 0, 1, 30, 9) == 7.25f);
    presentation::Scroll wheel(0, 2, .18f);
    wheel.advance(.09f);
    CHECK(wheel.value() > 0 && wheel.value() < 2);
    presentation::Scroll next(wheel.value(), 4, .18f);
    CHECK(next.value() == wheel.value());
    CHECK(next.advance(.18f) && next.value() == 4);
    std::vector<ArmyCardSlot> right, left;
    for (std::size_t i = 0; i < 9; ++i) {
        left.push_back({i, 119.5f + i * 149});
        right.push_back({8 + i, 119.5f + i * 149});
    }
    CHECK(armyDropIndex(7, 120.5f, right) == 8);
    CHECK(armyDropIndex(8, 119.5f + 7 * 149 - 1, left) == 7);
}
void testWholeFormationCompaction() {
    BattleSnapshot s;
    for (int team = 0; team < 2; ++team)
        for (int i = 0; i < 8; ++i)
            s.armies[team].units.push_back({UnitKind::Light, "Боец " + std::to_string(i),
                i == 0 || i == 3 ? 0 : 20, 20, 10});
    presentation::Playback playback;
    auto step = playback.begin(s, BattleEvent{EventType::Promote, 1, 0, "Боец 1"});
    CHECK(step.movesFormation());
    for (int team = 0; team < 2; ++team) {
        CHECK(s.armies[team].units.size() == 6 && s.armies[team].totalCost == 60);
        CHECK(step.previousIndices[team] == std::vector<std::size_t>({1,2,4,5,6,7}));
        for (std::size_t i = 0; i < 6; ++i)
            CHECK(presentation::formationIndex(step, team, i) == step.previousIndices[team][i]);
    }
    step.advance(presentation::duration(EventType::Promote) * .5f);
    CHECK(presentation::formationIndex(step, 0, 4) > 4 && presentation::formationIndex(step, 0, 4) < 6);
    step.advance(100);
    CHECK(presentation::formationIndex(step, 0, 4) == 4);
    CHECK(!playback.begin(s, BattleEvent{EventType::Promote, 1, 1, "Боец 1"}).movesFormation());
    // Deaths behind a surviving front have no core Promote event.
    s = frame();
    s.armies[0].units[2].hp = 0;
    auto cleanup = presentation::Playback::cleanup(s, 1);
    CHECK(cleanup.movesFormation() && s.armies[0].units.size() == 4);
    CHECK(cleanup.previousIndices[0] == std::vector<std::size_t>({0,1,3,4}));
    CHECK(presentation::formationIndex(cleanup, 0, 3) == 4);
    // A temporarily unknown clone is not a corpse just because HP isn't known.
    s.armies[0].units[1].detailsKnown = false;
    s.armies[0].units[1].hp = 0;
    CHECK(!presentation::Playback::cleanup(s).movesFormation() && s.armies[0].units.size() == 4);
    const auto death = playback.begin(s, BattleEvent{EventType::Death, 2, 0, "Мирн"});
    playback.commit(s, death);
    CHECK(presentation::Playback::cleanup(s).movesFormation() && s.armies[0].units.size() == 3);
}
void testArcherWithoutLivingTarget() {
    BattleSnapshot s;
    s.armies[0].units = {{UnitKind::Archer, "Дарр", 0, 16, 18},
                         {UnitKind::Archer, "Дарр", 16, 16, 18},
                         {UnitKind::Archer, "Дарр", 16, 16, 18}};
    s.armies[1].units = {{UnitKind::Light, "Мирн", 0, 20, 10},
                         {UnitKind::Light, "Брон", 20, 20, 10}};
    presentation::Playback playback;
    const BattleEvent miss{EventType::Miss, 1, 0, "Дарр"};
    auto melee = playback.begin(s, miss);
    CHECK(melee.actor.index == 0 && melee.visible);
    const auto blocked = playback.begin(s, miss);
    CHECK(blocked.actor.index == 1 && !blocked.visible);
    const auto shot = playback.begin(s, BattleEvent{EventType::Shot, 1, 0, "Дарр", "Брон", 2, 18});
    CHECK(shot.actor.index == 2 && shot.visible);
    const auto unknown = playback.begin(s, BattleEvent{EventType::Miss, 1, 0, "Неизвестная копия"});
    CHECK(!unknown.visible);
}

void testExactAndAmbiguousCloneSources() {
    struct DetailedClone : BattleEvent {
        int sourceIndex = -1;
        std::string cloneName;
        int cloneHp = -1;
    };
    for (int team = 0; team < 2; ++team) {
        BattleSnapshot s;
        s.armies[team].units = {{UnitKind::Archer, "Дарр", 20, 20, 18, 4, 0, 11},
                                {UnitKind::Archer, "Дарр", 1, 20, 18, 4, 0, 11},
                                {UnitKind::Mage, "Маг", 14, 14, 25}};
        DetailedClone event;
        event.type = EventType::Clone; event.team = team; event.turn = 1;
        event.actor = "Маг"; event.target = "Дарр";
        event.sourceIndex = 1; event.cloneName = "Дарр [Клон 1]"; event.cloneHp = 1;
        presentation::Playback playback;
        auto step = playback.begin(s, event);
        CHECK(step.target.index == 1 && step.clone->hp == 1 && step.clone->maxHp == 20);
        CHECK(step.clone->detailsKnown && step.clone->name == event.cloneName);
        CHECK(step.advance(presentation::duration(event.type) * .51f));
        playback.commit(s, step);
        CHECK(s.armies[team].units[2].hp == 1 && s.armies[team].units[2].name == event.cloneName);
        CHECK(presentation::actorSlot(step).index == 3 && presentation::targetSlot(step).index == 1);
        CHECK(s.armies[team].units[0].hp == 20 && s.armies[team].units[1].hp == 1);
    }
    auto s = frame();
    DetailedClone unset;
    unset.type = EventType::Clone; unset.team = 0; unset.turn = 1;
    unset.actor = "Зэн"; unset.target = "Дарр"; unset.sourceIndex = 0; unset.cloneHp = 0;
    presentation::Playback partial;
    const auto safe = partial.begin(s, unset);
    CHECK(safe.target.index == 2 && safe.clone->hp == 16);

    s.armies[0].units[1] = s.armies[0].units[2];
    s.armies[0].units[1].hp = 1;
    presentation::Playback playback;
    BattleEvent oldEvent{EventType::Clone, 1, 0, "Зэн", "Дарр"};
    const auto ambiguous = playback.begin(s, oldEvent);
    CHECK(!ambiguous.target.valid() && !ambiguous.clone->detailsKnown);
    playback.commit(s, ambiguous);
    CHECK(!s.armies[0].units[3].detailsKnown && s.armies[0].units[3].name == "Дарр [Клон 1]");
    s = frame();
    playback.reset();
    s.armies[0].clonesCreated = 8;
    auto numbered = playback.begin(s, oldEvent);
    CHECK(numbered.clone->name == "Дарр [Клон 9]" && numbered.clone->detailsKnown);
    s = frame();
    s.armies[0].units[0] = {UnitKind::Archer, "Дарр", 20, 20, 18};
    s.armies[0].units[2].hp = 1;
    s.armies[0].units[3].abilityRange = 1;
    playback.reset();
    const auto nearby = playback.begin(s, oldEvent);
    CHECK(nearby.target.index == 2 && nearby.clone->hp == 1 && nearby.clone->detailsKnown);
}
void testQuietCastKeepsCasterOrder() {
    BattleSnapshot s;
    s.armies[0].units = {{UnitKind::Light, "Дарр", 20, 20, 10},
                         {UnitKind::Mage, "Маг", 14, 14, 25}, {UnitKind::Mage, "Маг", 14, 14, 25}};
    presentation::Playback playback;
    BattleEvent failed{EventType::CloneFailed, 1, 0, "Маг", "Дарр"};
    CHECK(!visibleBattleEvent(failed.type));
    CHECK(playback.begin(s, failed).actor.index == 1);
    BattleEvent success{EventType::Clone, 1, 0, "Маг", "Дарр"};
    auto step = playback.begin(s, success);
    CHECK(step.actor.index == 2 && step.clone->name == "Дарр [Клон 1]");
    playback.commit(s, step);
    CHECK(s.armies[0].units[2].kind == UnitKind::Light && s.armies[0].clonesCreated == 1);
}
} // namespace
int main() {
    testPlaybackRatesAndScrolling();
    testContinuousArmyScrolling();
    testWholeFormationCompaction();
    testArcherWithoutLivingTarget();
    testExactAndAmbiguousCloneSources();
    testQuietCastKeepsCasterOrder();
    testTimingAndJournal();
    testEffectsChangeOnlySnapshots();
    testEquipmentStatsAtImpact();
    testDuplicateNamesAndReplayBoundary();
    std::cout << (failures ? "Presentation tests failed\n" : "Presentation tests passed\n");
    return failures ? 1 : 0;
}
