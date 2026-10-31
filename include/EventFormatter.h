#pragma once
#include <random>
#include <string>
#include "Event.h"

class EventFormatter {
public:
    static std::string format(const BattleEvent& event, std::mt19937& rng, bool withPrefix = true);
    static const char* teamName(int team);
};
