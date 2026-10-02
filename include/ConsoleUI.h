#pragma once
#include <cstdint>
#include <istream>
#include <memory>
#include <optional>
#include <ostream>
#include <random>
#include <string>
#include <vector>
#include "Army.h"
#include "Event.h"
#include "View.h"

class ConsoleUI {
public:
    ConsoleUI(std::istream& in, std::ostream& out);
    explicit ConsoleUI(View& view) : view_(&view) {}

    void setStage(ViewStage stage);
    std::optional<std::uint32_t> choose(const std::vector<MenuChoice>& choices);

    std::optional<std::uint32_t> readNumber(const std::string& prompt, std::uint32_t min,
                                          std::uint32_t max);
    void message(const std::string& text);
    void showArmies(Army& a, Army& b);
    void showEvents(const std::vector<BattleEvent>& events, std::mt19937& rng);
    void showResult(int winner);

private:
    std::unique_ptr<View> ownedView_;
    View* view_;
};
