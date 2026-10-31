#pragma once
#include "View.h"

// Position of a retained card before an edit; indices follow ArmyBuilder::move.
inline std::size_t previousArmyIndex(std::size_t now, const ArmyEdit& edit) {
    if (edit.kind == ArmyEdit::Kind::Remove)
        return now >= edit.from ? now + 1 : now;
    if (now == edit.to)
        return edit.from;
    if (edit.from < edit.to && now >= edit.from && now < edit.to)
        return now + 1;
    if (edit.from > edit.to && now > edit.to && now <= edit.from)
        return now - 1;
    return now;
}

struct ArmyCardSlot {
    std::size_t index;
    float center;
};

// Insert among the retained cards: moving right crosses the next card's center,
// moving left crosses the previous center. The source is excluded from the count.
inline std::optional<std::size_t> armyDropIndex(std::size_t from, float center,
                                                const std::vector<ArmyCardSlot>& visible) {
    if (visible.empty())
        return std::nullopt;
    std::size_t target = visible.front().index - (from < visible.front().index ? 1 : 0);
    for (const auto& card : visible) {
        if ((card.index < from && center >= card.center) || (card.index > from && center > card.center))
            ++target;
    }
    return target;
}
