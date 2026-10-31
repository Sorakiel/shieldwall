#pragma once
#include <algorithm>
#include <array>
#include <deque>
#include <string>
#include <utility>

namespace battletext {
// Unicode symbols are rendered by our SFML pictograms, independent of font coverage.
inline const std::array<std::pair<std::string, std::string>, 4> equipment{{
    {"шлем", "🪖"}, {"щит", "🛡"}, {"копьё", "🗡"}, {"конь", "🐎"}
}};

inline std::string icons(std::string text) {
    for (const auto& item : equipment) {
        const auto token = "[" + item.first + "]";
        std::size_t at = 0;
        while ((at = text.find(token, at)) != std::string::npos) {
            text.replace(at, token.size(), item.second);
            at += item.second.size();
        }
        const auto bonus = "бонус: " + item.first;
        if ((at = text.find(bonus)) != std::string::npos)
            text.replace(at, bonus.size(), "бонус: " + item.second);
    }
    return text;
}

inline std::pair<std::string, std::string> unitLabel(std::string name) {
    std::string badges;
    // Only decorator suffixes are equipment; brackets inside a custom name stay intact.
    while (!name.empty() && name.back() == ']') {
        bool found = false;
        for (const auto& item : equipment) {
            const auto token = " [" + item.first + "]";
            if (name.size() >= token.size() &&
                name.compare(name.size() - token.size(), token.size(), token) == 0) {
                name.resize(name.size() - token.size());
                badges = item.second + badges;
                found = true;
                break;
            }
        }
        if (!found) break;
    }
    return {name, badges};
}

struct Line {
    std::u32string text;
    int team = -1;
};

class Log {
public:
    std::deque<Line> lines;
    int lastTurn = -1;
    void clear() { lines.clear(); lastTurn = -1; }

    template<class Width>
    std::size_t startTurn(int turn, Width width, float limit) {
        if (lastTurn == turn) return 0;
        std::size_t added = lines.empty() ? 0 : append(U"\n", -1, width, limit);
        const auto digits = std::to_string(turn);
        added += append(U"Ход " + std::u32string(digits.begin(), digits.end()), -1, width, limit);
        lastTurn = turn;
        return added;
    }

    template<class Width>
    std::size_t append(const std::u32string& value, int team, Width width, float limit) {
        std::size_t added = 0;
        std::u32string line;
        auto flush = [&] {
            lines.push_back({line, team});
            if (lines.size() > 5000) lines.pop_front();
            ++added;
            line.clear();
        };
        for (const auto character : value) {
            if (character == U'\r') continue;
            if (character == U'\n') { flush(); continue; }
            line += character;
            if (line.size() > 1 && width(line) > limit) {
                auto split = line.find_last_of(U' ');
                if (split == std::u32string::npos || split == 0) split = line.size() - 1;
                const bool space = line[split] == U' ';
                const auto rest = line.substr(split + (space ? 1 : 0));
                line.resize(split);
                flush();
                line = rest;
            }
        }
        if (!line.empty()) flush();
        return added;
    }
};
} // namespace battletext
