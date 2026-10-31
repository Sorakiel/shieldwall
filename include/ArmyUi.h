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
