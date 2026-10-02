#include "ConsoleView.h"
#include <algorithm>

std::optional<std::uint32_t> ConsoleView::readNumber(const std::string& prompt,
                                                   std::uint32_t min, std::uint32_t max) {
    std::string line;
    while (true) {
        out_ << prompt << std::flush;
        if (!std::getline(in_, line)) {
            message("Ввод завершён. До встречи!");
            return std::nullopt;
        }
        if (const auto value = parseNumber(line, min, max)) return value;
        out_ << "Введите целое число от " << min << " до " << max << ".\n";
    }
}

std::optional<std::uint32_t> ConsoleView::choose(const std::vector<MenuChoice>& choices) {
    if (choices.empty()) return std::nullopt;
    for (const auto& choice : choices) {
        message(std::to_string(choice.value) + " — " + choice.label +
                (choice.enabled ? "" : " (недоступно)"));
    }
    const auto limits = std::minmax_element(choices.begin(), choices.end(),
        [](const MenuChoice& a, const MenuChoice& b) { return a.value < b.value; });
    while (const auto value = readNumber("> ", limits.first->value, limits.second->value)) {
        if (std::any_of(choices.begin(), choices.end(),
            [&](const MenuChoice& choice) { return choice.enabled && choice.value == *value; })) return value;
        message("Выберите доступный пункт меню.");
    }
    return std::nullopt;
}

void ConsoleView::message(const std::string& text) { out_ << text << '\n'; }

void ConsoleView::showArmies(const BattleSnapshot& snapshot) {
    for (const auto& army : snapshot.armies) {
        out_ << army.name << " — бойцов: " << army.units.size()
             << ", стоимость: " << army.totalCost << '\n';
        if (army.units.empty()) message("  Строй пуст.");
        for (std::size_t i = 0; i < army.units.size(); ++i) {
            const auto& unit = army.units[i];
            out_ << "  " << i + 1 << ". " << unitKindName(unit.kind) << " «" << unit.name
                 << "» | hp " << unit.hp << '/' << unit.maxHp
                 << " | цена " << unit.cost << '\n';
        }
    }
}

TurnAction ConsoleView::waitForTurn(BattleMode mode) {
    if (mode == BattleMode::Automatic) return TurnAction::Next;
    while (true) {
        message("Enter — следующий ход; a — автобой; 0 — сохранить и вернуться в меню.");
        std::string command;
        if (!std::getline(in_, command) || command == "0") return TurnAction::Exit;
        if (command.empty()) return TurnAction::Next;
        if (command == "a" || command == "A") return TurnAction::Automatic;
        message("Неизвестная команда.");
    }
}

bool ConsoleView::handoff(int team) {
    const std::string clear = "\033[2J\033[H\033[3J";
    out_ << clear;
    message("Передайте управление игроку " + std::to_string(team + 1) +
            ". Предыдущий игрок должен отвернуться. Нажмите Enter.");
    std::string line;
    while (std::getline(in_, line)) {
        if (line.empty()) {
            out_ << clear;
            return true;
        }
        message("Нажмите Enter без ввода команды.");
    }
    return false;
}

void ConsoleView::showRecruitment(const RecruitmentSnapshot& snapshot) {
    message("\nЗакупка: " + snapshot.armies[snapshot.activeTeam].name +
            " | бюджет " + std::to_string(snapshot.limit) +
            " | потрачено " + std::to_string(snapshot.spent) +
            " | осталось " + std::to_string(snapshot.remaining));
    const auto& army = snapshot.armies[snapshot.activeTeam];
    for (std::size_t i = 0; i < army.units.size(); ++i) {
        const auto& unit = army.units[i];
        message(std::to_string(i + 1) + ". " + unitKindName(unit.kind) + " «" + unit.name +
                "» | hp " + std::to_string(unit.maxHp) + " | цена " + std::to_string(unit.cost));
    }
    for (const auto& offer : snapshot.offers) {
        message(std::string(unitKindName(offer.kind)) + " | цена " + std::to_string(offer.cost) +
                " | hp " + std::to_string(offer.hp) +
                " | атака " + std::to_string(offer.melee) +
                " | выстрел " + std::to_string(offer.ranged) +
                " | дальность " + std::to_string(offer.range) +
                " | защита " + std::to_string(offer.defense));
    }
}
