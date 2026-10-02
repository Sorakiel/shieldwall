#include "ConsoleUI.h"
#include "ConsoleView.h"
#include "EventFormatter.h"

ConsoleUI::ConsoleUI(std::istream& in, std::ostream& out)
    : ownedView_(std::make_unique<ConsoleView>(in, out)), view_(ownedView_.get()) {}

void ConsoleUI::setStage(ViewStage stage) { view_->setStage(stage); }

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
    Army* armies[] = {&a, &b};
    for (int team = 0; team < 2; ++team) {
        auto& output = snapshot.armies[team];
        Army& army = *armies[team];
        output.name = EventFormatter::teamName(team);
        output.totalCost = army.totalCost();
        output.units.reserve(army.size());
        for (std::size_t i = 0; i < army.size(); ++i) {
            const Unit& unit = *army.at(i);
            output.units.push_back({unit.kind(), unit.name(), unit.hp(), unit.maxHp(), unit.cost()});
        }
    }
    snapshot.finished = a.isDefeated() || b.isDefeated();
    // Копии остаются валидными после removeDead() на следующем ходе.
    view_->showArmies(snapshot);
}

void ConsoleUI::showEvents(const std::vector<BattleEvent>& events, std::mt19937& rng) {
    for (const BattleEvent& event : events) message(EventFormatter::format(event, rng));
}

void ConsoleUI::showResult(int winner) {
    const std::string text = winner == 0 || winner == 1
        ? std::string("Итог: победили ") + EventFormatter::teamName(winner) + "."
        : "Итог: победитель не определён.";
    view_->showResult(winner, text);
}
