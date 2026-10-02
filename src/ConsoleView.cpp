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
        message(std::to_string(choice.value) + " — " + choice.label);
    }
    const auto limits = std::minmax_element(choices.begin(), choices.end(),
        [](const MenuChoice& a, const MenuChoice& b) { return a.value < b.value; });
    while (const auto value = readNumber("> ", limits.first->value, limits.second->value)) {
        if (std::any_of(choices.begin(), choices.end(),
            [&](const MenuChoice& choice) { return choice.value == *value; })) return value;
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
