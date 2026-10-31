#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "Event.h"
#include "View.h"

namespace presentation {
struct Slot {
    int team = -1;
    int index = -1;
    bool valid() const {
        return team >= 0 && team < 2 && index >= 0;
    }
};

inline float duration(EventType type) {
    switch (type) {
    case EventType::Melee:
        return .78f;
    case EventType::Shot:
        return .95f;
    case EventType::Heal:
        return 1.05f;
    case EventType::Clone:
        return 1.2f;
    case EventType::CloneFailed:
        return .75f;
    case EventType::Buff:
        return .9f;
    case EventType::Death:
        return .6f;
    case EventType::Promote:
        return .55f;
    case EventType::BattleEnd:
        return 1.0f;
    case EventType::Miss:
    case EventType::OutOfRange:
        return .42f;
    }
    return .7f;
}

struct Step {
    BattleEvent event;
    Slot actor, target;
    UnitKind subjectKind = UnitKind::Light;
    int fromIndex = -1;
    float elapsed = 0.f;
    bool committed = false;
    float progress() const {
        return std::clamp(elapsed / duration(event.type), 0.f, 1.f);
    }
    bool finished() const {
        return progress() >= 1.f;
    }
    // One impact per event, even if a slow frame crosses both impact and finish.
    bool advance(float seconds) {
        if (!std::isfinite(seconds) || seconds < 0.f)
            return false;
        elapsed = std::min(duration(event.type), elapsed + seconds);
        if (!committed && progress() >= .5f) {
            committed = true;
            return true;
        }
        return false;
    }
};

class Playback {
    int turn_ = -1;
    std::array<Slot, 2> lastHit_{};
    std::array<std::size_t, 2> shooters_{{1, 1}}, casters_{{1, 1}};
    static int find(const ArmySnapshot& army, const std::string& name, std::size_t start = 0,
                    int preferredHp = -1) {
        int fallback = -1;
        for (auto i = start; i < army.units.size(); ++i) {
            if (army.units[i].name != name)
                continue;
            if (fallback < 0)
                fallback = static_cast<int>(i);
            if (preferredHp < 0 || army.units[i].hp == preferredHp)
                return static_cast<int>(i);
        }
        return fallback;
    }

public:
    void reset() {
        turn_ = -1;
        lastHit_ = {};
        shooters_ = {{1, 1}};
        casters_ = {{1, 1}};
    }
    Step begin(BattleSnapshot& frame, const BattleEvent& event) {
        if (turn_ != event.turn) {
            reset();
            turn_ = event.turn;
        }
        Step step;
        step.event = event;
        if (event.team < 0 || event.team > 1)
            return step;
        const auto team = event.team;
        if (event.type == EventType::Promote) {
            step.fromIndex = find(frame.armies[team], event.actor);
            for (auto& army : frame.armies) {
                army.units.erase(std::remove_if(army.units.begin(), army.units.end(),
                                                [](const UnitSnapshot& unit) { return unit.hp <= 0; }),
                                 army.units.end());
            }
        }
        const bool shot = event.type == EventType::Shot || event.type == EventType::OutOfRange;
        const bool cast = event.type == EventType::Heal || event.type == EventType::Clone ||
                          event.type == EventType::CloneFailed;
        const auto start = shot ? shooters_[team] : cast ? casters_[team] : 0;
        int actor = find(frame.armies[team], event.actor, start, event.type == EventType::Death ? 0 : -1);
        if (event.type == EventType::Death && lastHit_[team].valid()) {
            const auto i = static_cast<std::size_t>(lastHit_[team].index);
            if (i < frame.armies[team].units.size() && frame.armies[team].units[i].name == event.actor)
                actor = lastHit_[team].index;
        }
        step.actor = {team, actor};
        if (actor >= 0 && shot)
            shooters_[team] = static_cast<std::size_t>(actor + 1);
        if (actor >= 0 && cast)
            casters_[team] = static_cast<std::size_t>(actor + 1);
        const bool enemy = event.type == EventType::Melee || event.type == EventType::Shot;
        const int targetTeam = enemy ? 1 - team : team;
        // Events identify units by copied names, not object addresses. Health helps
        // distinguish same-name targets; the authoritative next frame always wins.
        step.target = {targetTeam, find(frame.armies[targetTeam], event.target, 0,
                                        enemy ? event.hpLeft + event.damage : -1)};
        if (step.target.valid())
            step.subjectKind =
                frame.armies[targetTeam].units[static_cast<std::size_t>(step.target.index)].kind;
        return step;
    }
    void commit(BattleSnapshot& frame, const Step& step) {
        const auto& e = step.event;
        auto at = [&](Slot slot) -> UnitSnapshot* {
            if (!slot.valid() || static_cast<std::size_t>(slot.index) >= frame.armies[slot.team].units.size())
                return nullptr;
            return &frame.armies[slot.team].units[static_cast<std::size_t>(slot.index)];
        };
        if (e.type == EventType::Melee || e.type == EventType::Shot) {
            if (auto unit = at(step.target))
                unit->hp = std::max(0, e.hpLeft);
            if (step.target.valid())
                lastHit_[step.target.team] = step.target;
        } else if (e.type == EventType::Heal) {
            if (auto unit = at(step.target))
                unit->hp = std::min(unit->maxHp, unit->hp + e.restored);
        } else if (e.type == EventType::Death) {
            if (auto unit = at(step.actor))
                unit->hp = 0;
        } else if (e.type == EventType::Buff) {
            if (auto unit = at(step.target))
                unit->name += " [" + e.buff + "]";
        } else if (e.type == EventType::Clone) {
            const auto source = at(step.target);
            if (source && step.actor.valid()) {
                const auto copy = *source;
                auto& army = frame.armies[step.actor.team];
                army.units.insert(army.units.begin() + step.actor.index, copy);
                army.totalCost += copy.cost;
                ++casters_[step.actor.team];
            }
        }
    }
};
} // namespace presentation
