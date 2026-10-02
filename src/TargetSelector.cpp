#include "TargetSelector.h"
#include <algorithm>
#include <vector>

int FirstInLine::pick(Army& enemy, std::size_t maxPositions, std::mt19937&) const {
    if (enemy.size() == 0 || maxPositions == 0) return -1;
    return 0;
}

int RandomInReach::pick(Army& enemy, std::size_t maxPositions, std::mt19937& rng) const {
    const std::size_t limit = std::min(maxPositions, enemy.size());
    std::vector<std::size_t> live;
    live.reserve(limit);
    for (std::size_t i = 0; i < limit; ++i) {
        if (enemy.at(i)->isAlive()) live.push_back(i);
    }
    if (live.empty()) return -1;
    return static_cast<int>(live[rng() % live.size()]);
}