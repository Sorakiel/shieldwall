#pragma once
#include <cstddef>
#include <random>
#include "Army.h"

class TargetSelector {
public:
    virtual ~TargetSelector() = default;
    // Индекс цели в enemy или -1, если цели нет. maxPositions ограничивает зону.
    virtual int pick(Army& enemy, std::size_t maxPositions, std::mt19937& rng) const = 0;
};

class FirstInLine : public TargetSelector {
public:
    int pick(Army& enemy, std::size_t maxPositions, std::mt19937& rng) const override;
};

class RandomInReach : public TargetSelector {
public:
    int pick(Army& enemy, std::size_t maxPositions, std::mt19937& rng) const override;
};