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
#include "UserInterface.h"

class ConsoleUI final : public UserInterface {
public:
    ConsoleUI(std::istream& in, std::ostream& out);
    explicit ConsoleUI(View& view) : view_(&view) {}

    void setStage(ViewStage stage) override;
    void setBattleProgress(int turn, BattleMode mode) override;
    TurnAction waitForTurn(BattleMode mode) override;
    bool handoff(int team) override;
    void showRecruitment(const RecruitmentSnapshot& snapshot) override;
    std::optional<std::uint32_t> choose(const std::vector<MenuChoice>& choices) override;

    std::optional<std::uint32_t> readNumber(const std::string& prompt, std::uint32_t min,
                                          std::uint32_t max) override;
    void message(const std::string& text) override;
    void showArmies(Army& a, Army& b) override;
    void showEvents(const std::vector<BattleEvent>& events, std::mt19937& rng) override;
    void showResult(int winner) override;

private:
    std::unique_ptr<View> ownedView_;
    View* view_;
};
