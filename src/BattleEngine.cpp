#include "BattleEngine.h"

#include <algorithm>
#include "TargetSelector.h"
#include "Unit.h"

namespace {

    int damageOf(int attack, int defense) {
        const int raw = attack - defense;
        return raw < 1 ? 1 : raw;
    }

    int hpLeftOf(const Unit* unit) {
        return unit->hp() < 0 ? 0 : unit->hp();
    }

    // Один ближний удар. Атакующий — front своей армии. Если он мёртв — Miss.
    void meleeStrike(Army& attacker, Army& defender, int attackerTeam, int turn,
        std::vector<BattleEvent>& events) {
        Unit* actor = attacker.front();
        if (!actor) return;

        if (!actor->isAlive()) {
            events.push_back({ EventType::Miss, turn, attackerTeam, actor->name(), "", 0, 0 });
            return;
        }

        Unit* target = defender.front();
        if (!target) return;

        const int dmg = damageOf(actor->meleeAttack(), target->defense());
        target->takeDamage(dmg);
        events.push_back({ EventType::Melee, turn, attackerTeam,
                          actor->name(), target->name(), dmg, hpLeftOf(target) });
        if (!target->isAlive()) {
            events.push_back({ EventType::Death, turn, 1 - attackerTeam,
                              target->name(), "", 0, 0 });
        }
    }

    // Стрелки одной армии. Лучник на позиции 0 пропускается (он уже был в ближнем бою).
    void rangedStrike(Army& shooter, Army& target, int shooterTeam, int turn,
        std::mt19937& rng, std::vector<BattleEvent>& events) {
        RandomInReach selector;
        for (std::size_t i = 0; i < shooter.size(); ++i) {
            Unit* unit = shooter.at(i);
            if (!unit->isAlive()) continue;                  // убитый лучник не стреляет
            if (i == 0) continue;                            // лучник в первом ряду уже бил с руки
            Archer* archer = dynamic_cast<Archer*>(unit);
            if (!archer) continue;

            const int reach = archer->range() - static_cast<int>(i);
            if (reach < 1) {
                events.push_back({ EventType::OutOfRange, turn, shooterTeam,
                                  unit->name(), "", 0, 0 });
                continue;
            }

            const int pick = selector.pick(target, static_cast<std::size_t>(reach), rng);
            if (pick < 0) {
                events.push_back({ EventType::Miss, turn, shooterTeam,
                                  unit->name(), "", 0, 0 });
                continue;
            }

            Unit* tgt = target.at(static_cast<std::size_t>(pick));
            const int dmg = damageOf(archer->rangedAttack(), tgt->defense());
            tgt->takeDamage(dmg);
            events.push_back({ EventType::Shot, turn, shooterTeam,
                              unit->name(), tgt->name(), dmg, hpLeftOf(tgt) });
            if (!tgt->isAlive()) {
                events.push_back({ EventType::Death, turn, 1 - shooterTeam,
                                  tgt->name(), "", 0, 0 });
            }
        }
    }

    // Уборка + Promote для тех, кто вышел вперёд.
    void cleanup(Army& a, Army& b, int turn, std::vector<BattleEvent>& events) {
        Unit* aFrontBefore = a.size() > 0 ? a.at(0) : nullptr;
        Unit* bFrontBefore = b.size() > 0 ? b.at(0) : nullptr;

        a.removeDead();
        b.removeDead();

        if (a.size() > 0 && a.at(0) != aFrontBefore) {
            events.push_back({ EventType::Promote, turn, 0, a.at(0)->name(), "", 0, 0 });
        }
        if (b.size() > 0 && b.at(0) != bFrontBefore) {
            events.push_back({ EventType::Promote, turn, 1, b.at(0)->name(), "", 0, 0 });
        }
    }

}  // namespace

BattleEngine::BattleEngine(Army& a, Army& b, std::uint32_t seed)
    : a_(a), b_(b), rng_(seed) {
}

bool BattleEngine::finished() const {
    return a_.isDefeated() || b_.isDefeated();
}

int BattleEngine::winner() const {
    const bool aDead = a_.isDefeated();
    const bool bDead = b_.isDefeated();
    if (aDead && bDead) return -2;   // ничья (на практике почти невозможна)
    if (aDead) return 1;
    if (bDead) return 0;
    return -1;
}

std::vector<BattleEvent> BattleEngine::runToEnd() {
    std::vector<BattleEvent> all;
    while (!finished()) {
        auto turn = nextTurn();
        all.insert(all.end(), turn.begin(), turn.end());
    }
    return all;
}

std::vector<BattleEvent> BattleEngine::nextTurn() {
    std::vector<BattleEvent> events;
    if (finished()) return events;   // бой кончился — молча выходим

    ++turn_;

    // Жребий. rng() % 2 одинаково работает на MSVC и libc++/libstdc++.
    const bool aFirst = (rng_() % 2) == 0;

    // Фаза 1: ближний бой.
    if (aFirst) {
        meleeStrike(a_, b_, 0, turn_, events);
        meleeStrike(b_, a_, 1, turn_, events);
    }
    else {
        meleeStrike(b_, a_, 1, turn_, events);
        meleeStrike(a_, b_, 0, turn_, events);
    }

    // Фаза 2: стрелки.
    if (aFirst) {
        rangedStrike(a_, b_, 0, turn_, rng_, events);
        rangedStrike(b_, a_, 1, turn_, rng_, events);
    }
    else {
        rangedStrike(b_, a_, 1, turn_, rng_, events);
        rangedStrike(a_, b_, 0, turn_, rng_, events);
    }

    // Фаза 3: уборка.
    cleanup(a_, b_, turn_, events);

    if (finished()) {
        events.push_back({ EventType::BattleEnd, turn_, winner(), "", "", 0, 0 });
    }
    return events;
}