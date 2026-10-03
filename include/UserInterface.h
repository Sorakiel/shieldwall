#pragma once
#include <random>
#include "Army.h"
#include "Event.h"
#include "View.h"

// Контроллеры зависят от этого интерфейса; форматирование и отрисовка остаются снаружи.
class UserInterface {
public:
    virtual ~UserInterface() = default;
    virtual void setStage(ViewStage stage) = 0;
    virtual void setBattleProgress(int turn, BattleMode mode) = 0;
    virtual std::optional<std::string> readText(const std::string& prompt,
        std::size_t maxBytes) = 0;
    virtual std::optional<std::uint32_t> readNumber(const std::string& prompt,
        std::uint32_t min, std::uint32_t max) = 0;
    virtual std::optional<std::uint32_t> choose(const std::vector<MenuChoice>& choices) = 0;
    virtual void message(const std::string& text) = 0;
    virtual void showArmies(Army& a, Army& b) = 0;
    virtual void showRecruitment(const RecruitmentSnapshot& snapshot) = 0;
    virtual void showEvents(const std::vector<BattleEvent>& events, std::mt19937& rng) = 0;
    virtual void showResult(int winner) = 0;
    virtual TurnAction waitForTurn(BattleMode mode) = 0;
    virtual bool handoff(int team) = 0;
};
