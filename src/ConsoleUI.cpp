#include "ConsoleUI.h"
#include <charconv>
#include "EventFormatter.h"

namespace {

const char* kindName(UnitKind kind) {
    switch (kind) {
    case UnitKind::Light: return "Лёгкий";
    case UnitKind::Heavy: return "Тяжёлый";
    case UnitKind::Archer: return "Лучник";
    }
    return "Юнит";
}

}  // namespace

std::optional<std::uint32_t> ConsoleUI::readNumber(const std::string& prompt,
                                                 std::uint32_t min, std::uint32_t max) {
    std::string line;
    while (true) {
        out_ << prompt << std::flush;
        if (!std::getline(in_, line)) {
            message("Ввод завершён. До встречи!");
            return std::nullopt;
        }
        const auto first = line.find_first_not_of(" \t\r");
        if (first != std::string::npos) {
            const auto last = line.find_last_not_of(" \t\r");
            const char* begin = line.data() + first;
            const char* end = line.data() + last + 1;
            std::uint32_t value = 0;
            const auto parsed = std::from_chars(begin, end, value);
            if (parsed.ec == std::errc{} && parsed.ptr == end && value >= min && value <= max) {
                return value;
            }
        }
        out_ << "Введите целое число от " << min << " до " << max << ".\n";
    }
}

void ConsoleUI::message(const std::string& text) {
    out_ << text << '\n';
}

void ConsoleUI::showArmy(Army& army, int team) {
    out_ << EventFormatter::teamName(team) << " — бойцов: " << army.size()
         << ", стоимость: " << army.totalCost() << '\n';
    if (army.size() == 0) {
        message("  Строй пуст.");
    }
    for (std::size_t i = 0; i < army.size(); ++i) {
        const Unit* unit = army.at(i);
        out_ << "  " << i + 1 << ". " << kindName(unit->kind()) << " «" << unit->name()
             << "» | hp " << unit->hp() << '/' << unit->maxHp()
             << " | цена " << unit->cost() << '\n';
    }
}

void ConsoleUI::showArmies(Army& a, Army& b) {
    showArmy(a, 0);
    showArmy(b, 1);
}

void ConsoleUI::showEvents(const std::vector<BattleEvent>& events, std::mt19937& rng) {
    for (const BattleEvent& event : events) {
        message(EventFormatter::format(event, rng));
    }
}

void ConsoleUI::showResult(int winner) {
    if (winner == 0 || winner == 1) {
        message(std::string("Итог: победили ") + EventFormatter::teamName(winner) + ".");
    } else {
        message("Итог: победитель не определён.");
    }
}
