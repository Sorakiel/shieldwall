#pragma once
#include <array>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <vector>
#include "Unit.h"

enum class ViewStage { Setup, Recruitment, Battle, Result };

struct UnitSnapshot {
    UnitKind kind;
    std::string name;
    int hp;
    int maxHp;
    int cost;
};

struct ArmySnapshot {
    std::string name;
    int totalCost = 0;
    std::vector<UnitSnapshot> units;
};

struct BattleSnapshot {
    std::array<ArmySnapshot, 2> armies;
    bool finished = false;
};

struct MenuChoice {
    std::uint32_t value;
    std::string label;
};

// Закрытие окна прерывает синхронный runner на границе хода.
class ViewClosed : public std::exception {
public:
    const char* what() const noexcept override { return "Window closed"; }
};

class View {
public:
    virtual ~View() = default;
    virtual void setStage(ViewStage) {}
    virtual std::optional<std::uint32_t> readNumber(const std::string& prompt,
        std::uint32_t min, std::uint32_t max) = 0;
    virtual std::optional<std::uint32_t> choose(const std::vector<MenuChoice>& choices) = 0;
    virtual void message(const std::string& text) = 0;
    virtual void showArmies(const BattleSnapshot& snapshot) = 0;
    virtual void showResult(int, const std::string& text) { message(text); }
};

std::optional<std::uint32_t> parseNumber(const std::string& text,
                                       std::uint32_t min, std::uint32_t max);
const char* unitKindName(UnitKind kind);
