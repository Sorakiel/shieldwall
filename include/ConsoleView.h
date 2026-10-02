#pragma once
#include <istream>
#include <ostream>
#include "View.h"

class ConsoleView final : public View {
public:
    ConsoleView(std::istream& in, std::ostream& out) : in_(in), out_(out) {}
    std::optional<std::string> readText(const std::string& prompt,
        std::size_t maxBytes) override;
    std::optional<std::uint32_t> readNumber(const std::string& prompt,
        std::uint32_t min, std::uint32_t max) override;
    std::optional<std::uint32_t> choose(const std::vector<MenuChoice>& choices) override;
    TurnAction waitForTurn(BattleMode mode) override;
    bool handoff(int team) override;
    void showRecruitment(const RecruitmentSnapshot& snapshot) override;
    void message(const std::string& text) override;
    void showArmies(const BattleSnapshot& snapshot) override;

private:
    std::istream& in_;
    std::ostream& out_;
};
