#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "ConsoleView.h"
#include "BattleLog.h"
#include "ShortcutGuard.h"

namespace {
int failures = 0;
#define CHECK(value) do { if (!(value)) { ++failures; \
    std::cerr << "FAIL line " << __LINE__ << ": " << #value << '\n'; } } while (false)
using Rows = std::vector<std::vector<std::string>>;

std::vector<Rows> tables(const std::string& text) {
    std::vector<Rows> result;
    std::istringstream input(text);
    std::string line;
    bool inTable = false;
    while (std::getline(input, line)) {
        if (line.empty() || line.front() != '|') { inTable = false; continue; }
        if (!inTable) result.emplace_back();
        inTable = true;
        std::vector<std::string> row;
        std::size_t from = 1;
        for (auto to = line.find('|', from); to != std::string::npos; to = line.find('|', from)) {
            row.push_back(line.substr(from, to - from));
            from = to + 1;
        }
        result.back().push_back(std::move(row));
    }
    return result;
}

std::size_t letters(const std::string& value) {
    return static_cast<std::size_t>(std::count_if(value.begin(), value.end(),
        [](unsigned char c) { return (c & 0xc0) != 0x80; }));
}

void checkTable(const Rows& rows) {
    if (rows.empty()) { CHECK(false); return; }
    const auto columns = rows.front().size();
    std::vector<std::size_t> longest(columns);
    for (const auto& row : rows) {
        CHECK(row.size() == columns);
        for (std::size_t column = 0; column < row.size(); ++column) {
            const auto& cell = row[column];
            CHECK(letters(cell) == letters(rows.front()[column]));
            const auto left = cell.find_first_not_of(' ');
            if (left == std::string::npos) continue;
            const auto last = cell.find_last_not_of(' ');
            const auto right = cell.size() - last - 1;
            CHECK(left == right || right == left + 1);
            longest[column] = std::max(longest[column], letters(cell.substr(left, last - left + 1)));
        }
    }
    for (std::size_t column = 0; column < columns; ++column) {
        CHECK(letters(rows.front()[column]) == longest[column] + 2);
    }
}

RecruitmentSnapshot example() {
    RecruitmentSnapshot frame;
    frame.armies[0].name = "Синие";
    frame.limit = 120; frame.spent = 105; frame.remaining = 15;
    frame.armies[0].units = {
        {UnitKind::Light, "Дарр", 20, 20, 10}, {UnitKind::Heavy, "Хорд", 45, 45, 30},
        {UnitKind::Archer, "Орн", 16, 16, 18}, {UnitKind::Mage, "Зэн", 14, 14, 25},
        {UnitKind::Healer, "Торв", 18, 18, 22}
    };
    frame.offers = {
        {UnitKind::Light, 10, 20, 7, 0, 0, 1, true},
        {UnitKind::Heavy, 30, 45, 9, 0, 0, 5, false},
        {UnitKind::Archer, 18, 16, 4, 11, 3, 0, false},
        {UnitKind::Mage, 25, 14, 2, 60, 3, 0, false},
        {UnitKind::Healer, 22, 18, 2, 8, 3, 0, false}
    };
    return frame;
}

void testRecruitment() {
    auto frame = example();
    std::istringstream input("1\n");
    std::ostringstream output;
    ConsoleView view(input, output);
    view.showRecruitment(frame);
    CHECK(view.choose({{40, "Купить"}, {0, "В главное меню"}}) == 40u);
    const auto text = output.str();
    const auto rows = tables(text);
    CHECK(rows.size() == 2);
    if (rows.size() != 2) return;
    CHECK(rows[0].size() == 5 && rows[1].size() == 5);
    checkTable(rows[0]); checkTable(rows[1]);
    CHECK(rows[1].front().front() == "  Лёгкий  "); // width follows «Целитель», not UTF-8 bytes.
    CHECK(rows[1][3].front() == "   Маг    ");
    CHECK(text.find("осталось 15\n\n|") != std::string::npos);
    CHECK(text.find("|\n\n|") != std::string::npos);
    CHECK(text.find("|\n\n1 — Купить") != std::string::npos);
    CHECK(text.find("\n\n\n") == std::string::npos);

    std::istringstream none;
    std::ostringstream wider;
    ConsoleView other(none, wider);
    frame.armies[0].units[0].name = "Имя, которое длиннее всех остальных";
    frame.armies[0].units[0].cost = 123456;
    frame.offers[0].cost = 123456;
    frame.offers[4].ranged = 1234567890;
    other.showRecruitment(frame);
    const auto expanded = tables(wider.str());
    CHECK(expanded.size() == 2);
    if (expanded.size() != 2) return;
    checkTable(expanded[0]); checkTable(expanded[1]);
    CHECK(letters(expanded[0][0][0]) > letters(rows[0][0][0]));
    CHECK(letters(expanded[0][0][2]) > letters(rows[0][0][2]));
    CHECK(letters(expanded[1][0][1]) > letters(rows[1][0][1]));
    CHECK(letters(expanded[1][0][5]) > letters(rows[1][0][5]));

    std::ostringstream empty;
    ConsoleView emptyView(none, empty);
    frame.armies[0].units.clear();
    emptyView.showRecruitment(frame);
    CHECK(empty.str().find("\n\nСтрой пуст.\n\n|") != std::string::npos);
}

void testArmies() {
    BattleSnapshot frame;
    frame.armies[0] = example().armies[0];
    frame.armies[1].name = "Красные";
    frame.armies[1].units = {{UnitKind::Mage, "Длинное имя другого игрока", 3, 200, 9999}};
    std::istringstream input;
    std::ostringstream output;
    ConsoleView view(input, output);
    view.showArmies(frame);
    view.message("События хода.");
    const auto rows = tables(output.str());
    CHECK(rows.size() == 2);
    if (rows.size() != 2) return;
    Rows combined = rows[0];
    combined.insert(combined.end(), rows[1].begin(), rows[1].end());
    checkTable(combined);
    CHECK(output.str().find("hp 3/200") != std::string::npos);
    CHECK(output.str().find("|\n\nКрасные") != std::string::npos);
    CHECK(output.str().find("|\n\nСобытия хода.") != std::string::npos);
    CHECK(output.str().find("\n\n\n") == std::string::npos);
}

void testCloneNames() {
    BattleSnapshot frame;
    frame.armies[0].name = "Синие";
    frame.armies[0].units = {{UnitKind::Light, "Мирн", 20, 20, 10, 7, 1, 0, 12}};
    std::istringstream input;
    std::ostringstream output;
    ConsoleView view(input, output);
    view.showArmies(frame);
    CHECK(output.str().find("Мирн [Клон 12]") != std::string::npos);
    CHECK(frame.armies[0].units[0].name == "Мирн");
}
void testSaves() {
    std::istringstream input("2\n1\n5\n");
    std::ostringstream output;
    ConsoleView view(input, output);
    const std::vector<MenuChoice> choices{
        {11, "Короткое\nБой, ход 1 | лимит 20 | seed 0 | 01.01.2026"},
        {12, "Очень длинное имя\nЗакупка, ход 0 | лимит 12345 | seed 100 | 02.01.2026"},
        {13, "Битый файл\nПовреждён: нет данных", false},
        {20, "Назад", false}, {21, "Далее"}, {0, "Отмена"}
    };
    view.message("Сохранения");
    CHECK(view.choose(choices) == 12u);
    CHECK(view.choose(choices) == 11u);
    CHECK(view.choose(choices) == 21u);
    const auto blocks = tables(output.str());
    CHECK(blocks.size() == 3);
    for (const auto& block : blocks) checkTable(block);
    CHECK(output.str().find("|\n\n4 — Назад (недоступно)") != std::string::npos);
    CHECK(output.str().find("0 — Отмена") != std::string::npos);
}

void testWideAndCombiningNames() {
    std::istringstream input("0\n");
    std::ostringstream output;
    ConsoleView view(input, output);
    view.choose({{1, "Длинный\nБой"}, {2, "Ascii\nБой"},
                 {3, "日本語\nБой"}, {4, "е\xcc\x81\nБой"}, {0, "Отмена"}});
    const auto blocks = tables(output.str());
    CHECK(blocks.size() == 1 && blocks[0].size() == 4);
    if (blocks.size() != 1 || blocks[0].size() != 4) return;
    // Each first column takes 13 terminal columns (content width 11 + padding).
    CHECK(blocks[0][0][0] == " 1 — Длинный ");
    CHECK(blocks[0][1][0] == "  2 — Ascii  ");
    CHECK(blocks[0][2][0] == " 3 — 日本語  ");
    CHECK(blocks[0][3][0] == "    4 — е\xcc\x81    ");
}
void testTurnBlocksAndColors() {
    battletext::Log log;
    const auto width = [](const std::u32string& s) { return static_cast<float>(s.size()); };
    CHECK(log.startTurn(17, width, 20.f) == 1);
    log.append(U"Много слов для длинного синего события с переносами", 0, width, 20.f);
    CHECK(log.startTurn(17, width, 20.f) == 0);
    const auto redStart = log.lines.size();
    log.append(U"Красное событие\nс явным переносом и длинным продолжением", 1, width, 20.f);
    CHECK(log.lines.front().text == U"Ход 17");
    for (std::size_t i = 1; i < log.lines.size(); ++i) {
        CHECK(log.lines[i].team == (i < redStart ? 0 : 1));
        CHECK(width(log.lines[i].text) <= 20.f);
    }
    const auto boundary = log.lines.size();
    CHECK(log.startTurn(18, width, 20.f) == 2);
    CHECK(log.lines[boundary].text.empty());
    CHECK(log.lines[boundary + 1].text == U"Ход 18");
    CHECK(log.lines[boundary + 1].team == -1);
    log.append(U"Ничья", -2, width, 20.f);
    CHECK(log.lines.back().team == -2);
    log.clear();
    CHECK(log.lines.empty());
    CHECK(log.startTurn(18, width, 20.f) == 1);
    log.append(U"123456789012345678901234567890", 0, width, 20.f);
    for (const auto& line : log.lines) CHECK(width(line.text) <= 20.f);
    for (int i = 0; i < 5100; ++i) log.append(U"строка", 1, width, 20.f);
    CHECK(log.lines.size() == 5000);

    std::istringstream input;
    std::ostringstream output;
    ConsoleView view(input, output);
    view.battleMessage(17, 0, "Синий удар");
    view.battleMessage(17, 1, "Красный удар");
    view.battleMessage(18, 0, "Новый ход");
    CHECK(output.str() == "Ход 17\n[Синие] Синий удар\n[Красные] Красный удар\n\nХод 18\n[Синие] Новый ход\n");
    CHECK(output.str().find("\033[") == std::string::npos);
    view.clearDisplay();
    view.battleMessage(18, -2, "Ничья");
    CHECK(output.str().find("Ход 18\nНичья\n") != std::string::npos);
}

void testEquipmentLabels() {
    const auto label = battletext::unitLabel("Брон [конь] [щит] [шлем] [копьё]");
    CHECK(label.first == "Брон");
    CHECK(label.second == "🐎🛡🪖🗡");
    CHECK(battletext::icons("«Брон [конь] [щит] [шлем] [копьё]»") == "«Брон 🐎 🛡 🪖 🗡»");
    for (const auto& item : battletext::equipment) {
        CHECK(battletext::icons("бонус: " + item.first + ".") == "бонус: " + item.second + ".");
    }
    const auto custom = battletext::unitLabel("Имя [щит] внутри [неизвестно]");
    CHECK(custom.first == "Имя [щит] внутри [неизвестно]");
    CHECK(custom.second.empty());
}
void testColoredArmyBlocks() {
    BattleSnapshot frame;
    frame.armies[0] = {"Синие", 20, {{UnitKind::Light, "Мирн", 20, 20, 10},
                                    {UnitKind::Light, "Дарр", 16, 20, 10}}};
    frame.armies[1] = {"Красные", 30, {{UnitKind::Heavy, "Брон", 44, 45, 30}}};
    std::istringstream input;
    std::ostringstream colored, plain;
    ConsoleView colorView(input, colored, ConsoleColorMode::Always);
    ConsoleView plainView(input, plain, ConsoleColorMode::Never);
    colorView.showArmies(frame);
    plainView.showArmies(frame);
    colorView.message("Нейтральное сообщение");
    plainView.message("Нейтральное сообщение");
    const auto text = colored.str();
    const auto blueStart = text.find("\033[94mСиние");
    const auto blueEnd = text.find("\033[0m", blueStart);
    const auto redStart = text.find("\033[91mКрасные", blueEnd);
    const auto redEnd = text.find("\033[0m", redStart);
    CHECK(blueStart != std::string::npos && blueEnd != std::string::npos);
    CHECK(redStart != std::string::npos && redEnd != std::string::npos);
    if (blueStart == std::string::npos || blueEnd == std::string::npos ||
        redStart == std::string::npos || redEnd == std::string::npos) return;
    const auto blue = text.substr(blueStart, blueEnd - blueStart);
    const auto red = text.substr(redStart, redEnd - redStart);
    CHECK(blue.find("Мирн") != std::string::npos && blue.find("Дарр") != std::string::npos);
    CHECK(blue.find("hp 16/20") != std::string::npos && blue.find("цена 10") != std::string::npos);
    CHECK(red.find("Брон") != std::string::npos && red.find("hp 44/45") != std::string::npos);
    CHECK(text.find("Нейтральное сообщение") > redEnd);
    auto stripped = text;
    for (const auto code : {"\033[94m", "\033[91m", "\033[0m"}) {
        for (auto at = stripped.find(code); at != std::string::npos; at = stripped.find(code))
            stripped.erase(at, std::string(code).size());
    }
    CHECK(stripped == plain.str());
    CHECK(plain.str().find("\033[") == std::string::npos);
    frame.armies[0].units.clear();
    std::ostringstream empty;
    ConsoleView emptyView(input, empty, ConsoleColorMode::Always);
    emptyView.showArmies(frame);
    CHECK(empty.str().find("Строй пуст.\n\033[0m") != std::string::npos);
}

void testShortcutGestures() {
    ShortcutGuard guard;
    CHECK(ShortcutGuard::plain(true, false));
    CHECK(!ShortcutGuard::plain(true, true));
    CHECK(!ShortcutGuard::plain(false, false));
    CHECK(!guard.releaseSave(true, false)); // An orphan key-up is not a save.
    guard.pressSave(true, false);
    CHECK(guard.releaseSave(true, false));
    CHECK(!guard.releaseSave(true, false));
    guard.pressSave(true, true); // Win+Shift+S; modifiers can be released first.
    CHECK(!guard.releaseSave(true, false));
    guard.pressSave(true, true);
    CHECK(!guard.releaseSave(true, true));
    guard.pressSave(true, false);
    guard.cancel(); // A modifier was pressed after S, then released before S.
    CHECK(!guard.releaseSave(true, false));
    guard.pressSave(true, false);
    CHECK(!guard.releaseSave(true, true));
    guard.pressSave(true, false);
    guard.cancel(); // Focus lost and restored while S was down.
    CHECK(!guard.releaseSave(true, false));
    guard.pressSave(false, false);
    CHECK(!guard.releaseSave(true, false));
    guard.pressSave(true, false);
    CHECK(!guard.releaseSave(false, false));
    guard.pressSave(true, false); // A fresh plain press still works after cancellation.
    CHECK(guard.releaseSave(true, false));
}
}  // namespace

int main() {
    testColoredArmyBlocks();
    testShortcutGestures();
    testTurnBlocksAndColors();
    testEquipmentLabels();
    testRecruitment();
    testArmies();
    testCloneNames();
    testSaves();
    testWideAndCombiningNames();
    std::cout << (failures ? "Console layout tests failed\n" : "Console layout tests passed\n");
    return failures ? 1 : 0;
}
