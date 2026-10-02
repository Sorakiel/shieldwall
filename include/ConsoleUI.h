#pragma once
#include <cstdint>
#include <istream>
#include <optional>
#include <ostream>
#include <random>
#include <string>
#include <vector>
#include "Army.h"
#include "Event.h"

class ConsoleUI {
public:
    ConsoleUI(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

    std::optional<std::uint32_t> readNumber(const std::string& prompt, std::uint32_t min,
                                          std::uint32_t max);
    void message(const std::string& text);
    void showArmies(Army& a, Army& b);
    void showEvents(const std::vector<BattleEvent>& events, std::mt19937& rng);
    void showResult(int winner);

private:
    void showArmy(Army& army, int team);

    std::istream& in_;
    std::ostream& out_;
};
