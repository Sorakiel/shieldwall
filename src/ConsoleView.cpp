#include "ConsoleView.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
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
using Table = std::vector<std::vector<std::string>>;

std::string cellText(std::string value) {
    // Keep a cell on one terminal line, even if a loaded name/error contains controls.
    for (auto& byte : value) {
        if (static_cast<unsigned char>(byte) < 0x20 || byte == 0x7f) byte = ' ';
    }
    return value;
}

std::vector<std::string> splitCells(const std::string& text) {
    std::vector<std::string> result;
    std::size_t from = 0;
    while (true) {
        const auto to = text.find('|', from);
        auto value = text.substr(from, to == std::string::npos ? to : to - from);
        const auto first = value.find_first_not_of(" \t\r\n");
        const auto last = value.find_last_not_of(" \t\r\n");
        result.push_back(first == std::string::npos ? "" : cellText(value.substr(first, last - first + 1)));
        if (to == std::string::npos) return result;
        from = to + 1;
    }
}

std::size_t terminalWidth(const std::string& text) {
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i++]);
        char32_t point = lead;
        const int extra = lead >= 0xf0 && lead <= 0xf4 ? 3 :
                          lead >= 0xe0 && lead <= 0xef ? 2 :
                          lead >= 0xc2 && lead <= 0xdf ? 1 : 0;
        if (extra > 0) {
            const auto start = i;
            point = lead & (0x7f >> (extra + 1));
            bool valid = i + static_cast<std::size_t>(extra) <= text.size();
            for (int n = 0; valid && n < extra; ++n) {
                const auto byte = static_cast<unsigned char>(text[i++]);
                if ((byte & 0xc0) != 0x80) valid = false;
                else point = (point << 6) | (byte & 0x3f);
            }
            if (!valid || (extra == 1 && point < 0x80) || (extra == 2 && point < 0x800) ||
                (extra == 3 && point < 0x10000) || point > 0x10ffff ||
                (point >= 0xd800 && point <= 0xdfff)) { point = 0xfffd; i = start; }
        }
        const bool combining = (point >= 0x300 && point <= 0x36f) ||
            (point >= 0x1ab0 && point <= 0x1aff) || (point >= 0x1dc0 && point <= 0x1dff) ||
            (point >= 0x20d0 && point <= 0x20ff) || (point >= 0xfe00 && point <= 0xfe0f) ||
            (point >= 0xfe20 && point <= 0xfe2f);
        const bool wide = (point >= 0x1100 && point <= 0x115f) ||
            (point >= 0x2e80 && point <= 0xa4cf && point != 0x303f) ||
            (point >= 0xac00 && point <= 0xd7a3) || (point >= 0xf900 && point <= 0xfaff) ||
            (point >= 0xfe10 && point <= 0xfe19) || (point >= 0xfe30 && point <= 0xfe6f) ||
            (point >= 0xff01 && point <= 0xff60) || (point >= 0xffe0 && point <= 0xffe6) ||
            (point >= 0x20000 && point <= 0x3fffd);
        width += combining ? 0 : wide ? 2 : 1;
    }
    return width;
}

std::vector<std::size_t> columnWidths(const Table& rows) {
    std::vector<std::size_t> widths;
    for (const auto& row : rows) {
        widths.resize(std::max(widths.size(), row.size()));
        for (std::size_t i = 0; i < row.size(); ++i) {
            widths[i] = std::max(widths[i], terminalWidth(cellText(row[i])));
        }
    }
    return widths;
}

void printTable(ConsoleView& view, const Table& rows, const std::vector<std::size_t>& widths) {
    for (const auto& row : rows) {
        std::string line = "|";
        for (std::size_t i = 0; i < widths.size(); ++i) {
            const auto value = i < row.size() ? cellText(row[i]) : "";
            const auto padding = widths[i] - terminalWidth(value);
            line += " " + std::string(padding / 2, ' ') + value +
                    std::string(padding - padding / 2, ' ') + " |";
        }
        view.message(line);
    }
}

Table armyRows(const ArmySnapshot& army, bool recruitment) {
    Table rows;
    for (std::size_t index = 0; index < army.units.size(); ++index) {
        const auto& unit = army.units[index];
        rows.push_back({std::to_string(index + 1) + ". " + unitKindName(unit.kind) + " «" + unit.name + "»",
            "hp " + (recruitment ? std::to_string(unit.maxHp) :
                std::to_string(unit.hp) + "/" + std::to_string(unit.maxHp)),
            "цена " + std::to_string(unit.cost)});
    }
    return rows;
}

bool interactiveInput(std::istream& input) {
#ifdef _WIN32
    return &input == &std::cin && _isatty(_fileno(stdin)) != 0;
#else
    return &input == &std::cin && isatty(STDIN_FILENO);
#endif
}
bool colorOutput(std::ostream& output) {
    if (&output != &std::cout) return false;
#ifdef _WIN32
    if (GetEnvironmentVariableW(L"NO_COLOR", nullptr, 0) != 0 ||
        GetLastError() != ERROR_ENVVAR_NOT_FOUND) return false;
    DWORD mode = 0;
    const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
    return GetConsoleMode(handle, &mode) && (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    if (std::getenv("NO_COLOR")) return false;
    const auto term = std::getenv("TERM");
    return isatty(STDOUT_FILENO) && term && std::string(term) != "dumb";
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
    separateBlock();
    message("/cancel — Отмена; пустое имя — предложенное имя сохранения.");
    while (true) {
        out_ << prompt << std::flush;
        lastLineBlank_ = false;
        if (!std::getline(in_, line)) return std::nullopt;
        if (line == "/cancel") return std::nullopt;
        if (line.size() <= maxBytes) return line;
        message("Имя слишком длинное: максимум " + std::to_string(maxBytes) + " байт UTF-8.");
    }
}

void ConsoleView::battleMessage(int turn, int team, const std::string& text) {
    if (logTurn_ != turn) {
        separateBlock();
        message("Ход " + std::to_string(turn));
        logTurn_ = turn;
    }
    const bool color = colorOutput(out_) && (team == 0 || team == 1);
    if (color) out_ << (team == 0 ? "\033[94m" : "\033[91m");
    const auto label = team == 0 ? "[Синие] " : team == 1 ? "[Красные] " : "";
    // Color every physical line, including explicit newlines in event text.
    if (color) message(text);
    else message(std::string(label) + text);
    if (color) out_ << "\033[0m";
}

void ConsoleView::clearDisplay() {
    out_ << "\033[2J\033[H\033[3J" << std::flush;
    lastLineBlank_ = true;
    logTurn_ = -1;
}

std::optional<std::uint32_t> ConsoleView::readNumberCancelable(const std::string& prompt,
    std::uint32_t min, std::uint32_t max, NumberCancel cancel) {
    separateBlock();
    message(cancel == NumberCancel::Dialog ? "0 — Отмена" : "/menu — В главное меню");
    std::string line;
    while (true) {
        out_ << prompt << std::flush;
        lastLineBlank_ = false;
        if (!std::getline(in_, line)) return std::nullopt;
        if (cancel == NumberCancel::MainMenu && line == "/menu") return std::nullopt;
        if (cancel == NumberCancel::Dialog && parseNumber(line, 0, 0)) return std::nullopt;
        if (const auto value = parseNumber(line, min, max)) return value;
        message("Введите целое число от " + std::to_string(min) + " до " + std::to_string(max) + ".");
    }
}

std::optional<std::uint32_t> ConsoleView::readNumber(const std::string& prompt,
                                                   std::uint32_t min, std::uint32_t max) {
    std::string line;
    while (true) {
        out_ << prompt << std::flush;
        lastLineBlank_ = false;
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
    separateBlock();
    // Display positions are separate from controller action IDs (20, 30, 40, ...).
    std::vector<const MenuChoice*> numbered;
    const MenuChoice* zero = nullptr;
    Table entries;
    for (const auto& choice : choices) {
        if (choice.value == 0) { zero = &choice; continue; }
        numbered.push_back(&choice);
        const auto split = choice.label.find('\n');
        if (split != std::string::npos) {
            auto cells = splitCells(choice.label.substr(split + 1));
            cells.insert(cells.begin(), std::to_string(numbered.size()) + " — " +
                choice.label.substr(0, split) + (choice.enabled ? "" : " (недоступно)"));
            entries.push_back(std::move(cells));
        }
    }
    if (!entries.empty()) {
        printTable(*this, entries, columnWidths(entries));
        separateBlock();
    }
    for (std::size_t i = 0; i < numbered.size(); ++i) {
        const auto& choice = *numbered[i];
        if (choice.label.find('\n') == std::string::npos) {
            message(std::to_string(i + 1) + " — " + choice.label +
                    (choice.enabled ? "" : " (недоступно)"));
        }
    }
    if (zero) message("0 — " + zero->label + (zero->enabled ? "" : " (недоступно)"));
    while (const auto value = readNumber("> ", zero ? 0u : 1u,
                                        static_cast<std::uint32_t>(numbered.size()))) {
        const MenuChoice* choice = *value == 0 ? zero : numbered[*value - 1];
        if (choice && choice->enabled) return choice->value;
        message("Выберите доступный пункт меню.");
    }
    return std::nullopt;
}

void ConsoleView::message(const std::string& text) {
    out_ << text << '\n';
    lastLineBlank_ = text.empty() || text.back() == '\n';
}

void ConsoleView::separateBlock() {
    if (!lastLineBlank_) message("");
}

void ConsoleView::showArmies(const BattleSnapshot& snapshot) {
    std::array<Table, 2> tables;
    Table all;
    for (std::size_t i = 0; i < tables.size(); ++i) {
        tables[i] = armyRows(snapshot.armies[i], false);
        all.insert(all.end(), tables[i].begin(), tables[i].end());
    }
    const auto widths = columnWidths(all);
    for (std::size_t i = 0; i < tables.size(); ++i) {
        const auto& army = snapshot.armies[i];
        separateBlock();
        message(army.name + " — бойцов: " + std::to_string(army.units.size()) +
                ", стоимость: " + std::to_string(army.totalCost));
        separateBlock();
        if (tables[i].empty()) message("Строй пуст.");
        else printTable(*this, tables[i], widths);
    }
    separateBlock();
}

TurnAction ConsoleView::waitForTurn(BattleMode mode) {
    // Автобой в перенаправленном вводе не съедает команды следующего меню.
    if (mode == BattleMode::Automatic && !interactiveInput(in_)) return TurnAction::Next;
    if (mode == BattleMode::Automatic) {
        separateBlock();
        message("Автобой: s + Enter — сохранить; m + Enter — по ходам; 0 + Enter — меню.");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(550);
        while (!inputReady()) {
            if (std::chrono::steady_clock::now() >= deadline) return TurnAction::Next;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    while (true) {
        separateBlock();
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
    clearDisplay();
    message("Передайте управление игроку " + std::to_string(team + 1) +
            ". Предыдущий игрок должен отвернуться. Нажмите Enter.");
    std::string line;
    while (std::getline(in_, line)) {
        if (line.empty()) {
            clearDisplay();
            return true;
        }
        message("Нажмите Enter без ввода команды.");
    }
    return false;
}

void ConsoleView::showRecruitment(const RecruitmentSnapshot& snapshot) {
    separateBlock();
    message("Закупка: " + snapshot.armies[snapshot.activeTeam].name +
            " | бюджет " + std::to_string(snapshot.limit) +
            " | потрачено " + std::to_string(snapshot.spent) +
            " | осталось " + std::to_string(snapshot.remaining));
    separateBlock();
    const auto rows = armyRows(snapshot.armies[snapshot.activeTeam], true);
    if (rows.empty()) message("Строй пуст.");
    else printTable(*this, rows, columnWidths(rows));
    separateBlock();
    Table offers;
    for (const auto& offer : snapshot.offers) {
        std::vector<std::string> cells{unitKindName(offer.kind),
            "цена " + std::to_string(offer.cost), "hp " + std::to_string(offer.hp),
            "атака " + std::to_string(offer.melee), "защита " + std::to_string(offer.defense)};
        const auto special = splitCells(specialActionDescription(offer.kind, offer.ranged, offer.range));
        cells.insert(cells.end(), special.begin(), special.end());
        offers.push_back(std::move(cells));
    }
    printTable(*this, offers, columnWidths(offers));
    separateBlock();
}
