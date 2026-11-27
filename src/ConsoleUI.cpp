#include "ConsoleUI.h"
#include "ConsoleView.h"
#include "EventFormatter.h"
#include "CloneIdentity.h"
#include "CloneEventData.h"
#include "Mage.h"
#include <utility>

ConsoleUI::ConsoleUI(std::istream& in, std::ostream& out)
    : ownedView_(std::make_unique<ConsoleView>(in, out)), view_(ownedView_.get()) {
}

void ConsoleUI::setPlaybackHandler(PlaybackHandler handler) { view_->setPlaybackHandler(std::move(handler)); }
std::optional<ArmyEdit> ConsoleUI::takeArmyEdit() { return view_->takeArmyEdit(); }
std::optional<std::uint32_t> ConsoleUI::readSetupNumber(const std::string& prompt,
    std::uint32_t min, std::uint32_t max, NumberCancel cancel, std::uint32_t initial) {
    return view_->readSetupNumber(prompt, min, max, cancel, initial);
}

void ConsoleUI::setStage(ViewStage stage) {
    if (stage == ViewStage::MainMenu) {
        cloneCounts_ = {};
        eventFrame_ = {};
        eventPlayback_.reset();
    }
    view_->setStage(stage);
}
void ConsoleUI::clearDisplay() { view_->clearDisplay(); }

std::optional<std::uint32_t> ConsoleUI::readNumberCancelable(const std::string& prompt,
    std::uint32_t min, std::uint32_t max, NumberCancel cancel) {
    return view_->readNumberCancelable(prompt, min, max, cancel);
}

void ConsoleUI::setBattleProgress(int turn, BattleMode mode) {
    view_->setBattleProgress(turn, mode);
}

TurnAction ConsoleUI::waitForTurn(BattleMode mode) { return view_->waitForTurn(mode); }
bool ConsoleUI::handoff(int team) { return view_->handoff(team); }

void ConsoleUI::showRecruitment(const RecruitmentSnapshot& snapshot) {
    view_->showRecruitment(snapshot);
}

std::optional<std::uint32_t> ConsoleUI::choose(const std::vector<MenuChoice>& choices) {
    return view_->choose(choices);
}

std::optional<std::uint32_t> ConsoleUI::readNumber(const std::string& prompt,
    std::uint32_t min, std::uint32_t max) {
    return view_->readNumber(prompt, min, max);
}

std::optional<std::string> ConsoleUI::readText(const std::string& prompt, std::size_t maxBytes) {
    return view_->readText(prompt, maxBytes);
}

void ConsoleUI::message(const std::string& text) { view_->message(text); }

void ConsoleUI::showArmies(Army& a, Army& b) {
    BattleSnapshot snapshot;
    Army* armies[] = { &a, &b };
    for (int team = 0; team < 2; ++team) {
        auto& output = snapshot.armies[team];
        Army& army = *armies[team];
        output.name = EventFormatter::teamName(team);
        output.totalCost = army.totalCost();
        output.units.reserve(army.size());
        for (std::size_t i = 0; i < army.size(); ++i) {
            const Unit& unit = *army.at(i);
            const auto* archer = dynamic_cast<const Archer*>(&unit);
            output.clonesCreated = std::max(output.clonesCreated, cloneidentity::parse(unit.name()).number);
            const auto* mage = dynamic_cast<const Mage*>(&unit);
            if (mage) output.clonesCreated = std::max(output.clonesCreated, mage->clonesCreated());
            output.units.push_back({unit.kind(), unit.name(), unit.hp(), unit.maxHp(), unit.cost(),
                                    unit.meleeAttack(), unit.defense(), archer ? archer->rangedAttack() : 0,
                                    true, mage ? mage->range() : archer ? archer->range() : 0});
        }
    }
    for (int team = 0; team < 2; ++team) cloneCounts_[team] = snapshot.armies[team].clonesCreated;
    snapshot.finished = a.isDefeated() || b.isDefeated();
    eventFrame_ = snapshot;
    eventPlayback_.reset();
    // Копии остаются валидными после removeDead() на следующем ходе.
    view_->showArmies(snapshot);
}

void ConsoleUI::showEvents(const std::vector<BattleEvent>& events, std::mt19937& rng) {
    for (const BattleEvent& event : events) {
        std::string cloneName;
        if (event.type == EventType::Clone && event.team >= 0 && event.team < 2) {
            const auto data = presentation::cloneData(event);
            auto& number = cloneCounts_[static_cast<std::size_t>(event.team)];
            ++number;
            cloneName = data.name.empty() ? cloneidentity::numbered(event.target, number) : data.name;
            number = std::max(number, cloneidentity::parse(cloneName).number);
        }
        // Formatting every event preserves the separate journal RNG sequence.
        const auto step = eventPlayback_.begin(eventFrame_, event);
        eventPlayback_.commit(eventFrame_, step);
        view_->presentBattleEvent(event, EventFormatter::format(event, rng, false, cloneName), step.visible);
    }
    view_->finishBattleEvents();
}

void ConsoleUI::showResult(int winner) {
    const std::string text = winner == 0 || winner == 1
        ? std::string("Итог: победили ") + EventFormatter::teamName(winner) + "."
        : winner == -2 ? "Итог: ничья." : "Итог: победитель не определён.";
    view_->showResult(winner, text);
}
