#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "ConsoleView.h"

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
    frame.offers[4].ranged = 123456;
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
}  // namespace

int main() {
    testRecruitment();
    testArmies();
    testSaves();
    testWideAndCombiningNames();
    std::cout << (failures ? "Console layout tests failed\n" : "Console layout tests passed\n");
    return failures ? 1 : 0;
}
