#pragma once
#include <array>
#include <cstdint>
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Unit.h"
#include "Event.h"

enum class ViewStage { MainMenu, Setup, Recruitment, Battle, Result };
enum class NumberCancel { MainMenu, Dialog, Previous };
namespace uiAction {
inline constexpr std::uint32_t Back = 0x70000000u, ArmyEdit = 0x70000001u, ClearArmy = 0x70000002u;
}
enum class RecruitmentScreen { Methods, Academy };
struct ArmyEdit {
    enum class Kind { Remove, Move } kind = Kind::Remove;
    std::size_t from = 0, to = 0;
};
class ViewBack : public std::exception {};
class PlaybackStopped : public std::exception {};

enum class BattleMode { Automatic, Manual };
enum class TurnAction { Next, Automatic, Manual, Exit, Save };
using PlaybackHandler = std::function<bool(TurnAction)>;

struct UnitSnapshot {
    UnitKind kind;
    std::string name;
    int hp;
    int maxHp;
    int cost;
    int attack = 0;
    int defense = 0;
    int rangedAttack = 0;
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
    bool enabled = true;
    bool escapeCancel = false;
};

struct RecruitmentOffer {
    UnitKind kind;
    int cost;
    int hp;
    int melee;
    int ranged;
    int range;
    int defense;
    bool enabled;
};

struct RecruitmentSnapshot {
    RecruitmentScreen screen = RecruitmentScreen::Methods;
    std::array<ArmySnapshot, 2> armies;
    std::vector<RecruitmentOffer> offers;
    int activeTeam = 0;
    int limit = 0;
    int spent = 0;
    int remaining = 0;
};

// Закрытие окна прерывает синхронный runner на границе хода.
class ViewClosed : public std::exception {
public:
    const char* what() const noexcept override { return "Window closed"; }
};

class View {
public:
    virtual ~View() = default;
    virtual void setPlaybackHandler(PlaybackHandler) {}
    virtual std::optional<ArmyEdit> takeArmyEdit() { return std::nullopt; }
    virtual std::optional<std::uint32_t> readSetupNumber(const std::string& prompt,
        std::uint32_t min, std::uint32_t max, NumberCancel cancel, std::uint32_t) {
        return readNumberCancelable(prompt, min, max, cancel);
    }
    virtual void setStage(ViewStage) {}
    virtual void clearDisplay() {}
    virtual void setBattleProgress(int, BattleMode) {}
    virtual TurnAction waitForTurn(BattleMode) { return TurnAction::Next; }
    virtual bool handoff(int) { return true; }
    virtual void showRecruitment(const RecruitmentSnapshot& snapshot) {
        BattleSnapshot frame;
        frame.armies = snapshot.armies;
        showArmies(frame);
    }
    virtual std::optional<std::string> readText(const std::string&, std::size_t) {
        return std::nullopt;
    }
    virtual std::optional<std::uint32_t> readNumber(const std::string& prompt,
        std::uint32_t min, std::uint32_t max) = 0;
    virtual std::optional<std::uint32_t> readNumberCancelable(const std::string& prompt,
        std::uint32_t min, std::uint32_t max, NumberCancel) {
        return readNumber(prompt, min, max);
    }
    virtual std::optional<std::uint32_t> choose(const std::vector<MenuChoice>& choices) = 0;
    virtual void message(const std::string& text) = 0;
    // Team/turn metadata survives wrapping and is independent of translated text.
    virtual void battleMessage(int turn, int team, const std::string& text) {
        message("[Ход " + std::to_string(turn) + "] [" +
            (team == 0 ? "Синие" : team == 1 ? "Красные" : "Без команды") + "] " + text);
    }
    virtual void battleEvent(const BattleEvent& event, const std::string& text) {
        battleMessage(event.turn, event.team, text);
    }
    virtual void showArmies(const BattleSnapshot& snapshot) = 0;
    virtual void showResult(int, const std::string& text) { message(text); }
};

std::optional<std::uint32_t> parseNumber(const std::string& text,
                                       std::uint32_t min, std::uint32_t max);
const char* unitKindName(UnitKind kind);

std::string counted(std::size_t count, const char* one, const char* few, const char* many);
std::string unitCount(std::size_t count);
std::string moneyCount(std::size_t count);
std::string specialActionDescription(UnitKind kind, int power, int range);
