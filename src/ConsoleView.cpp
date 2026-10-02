#include "ConsoleView.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>
#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <sys/select.h>
#include <unistd.h>
#endif

namespace {
bool interactiveInput(std::istream& input) {
#ifdef _WIN32
    return &input == &std::cin && _isatty(_fileno(stdin)) != 0;
#else
    return &input == &std::cin && isatty(STDIN_FILENO);
#endif
}
bool inputReady() {
#ifdef _WIN32
    return _kbhit() != 0;
#else
    fd_set descriptors;
    FD_ZERO(&descriptors);
    FD_SET(STDIN_FILENO, &descriptors);
    timeval timeout{};
    return select(STDIN_FILENO + 1, &descriptors, nullptr, nullptr, &timeout) > 0;
#endif
}
}  // namespace

std::optional<std::string> ConsoleView::readText(const std::string& prompt, std::size_t maxBytes) {
    std::string line;
    while (true) {
        out_ << prompt << std::flush;
        if (!std::getline(in_, line)) return std::nullopt;
        if (line.size() <= maxBytes) return line;
        message("Имя слишком длинное: максимум " + std::to_string(maxBytes) + " байт UTF-8.");
    }
}

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
    // Автобой в перенаправленном вводе не съедает команды следующего меню.
    if (mode == BattleMode::Automatic && !interactiveInput(in_)) return TurnAction::Next;
    if (mode == BattleMode::Automatic) {
        message("Автобой: s + Enter — сохранить; m + Enter — по ходам; 0 + Enter — меню.");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(550);
        while (!inputReady()) {
            if (std::chrono::steady_clock::now() >= deadline) return TurnAction::Next;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    while (true) {
        message("Enter — следующий ход; a — автобой; s — сохранить; 0 — в главное меню.");
        std::string command;
        if (!std::getline(in_, command) || command == "0") return TurnAction::Exit;
        if (command.empty()) return TurnAction::Next;
        if (command == "a" || command == "A") return TurnAction::Automatic;
        if (command == "m" || command == "M") return TurnAction::Manual;
        if (command == "s" || command == "S") return TurnAction::Save;
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
