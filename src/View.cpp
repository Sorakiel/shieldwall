#include "View.h"
#include <charconv>

std::optional<std::uint32_t> parseNumber(const std::string& text,
                                       std::uint32_t min, std::uint32_t max) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) return std::nullopt;
    const auto last = text.find_last_not_of(" \t\r");
    const char* begin = text.data() + first;
    const char* end = text.data() + last + 1;
    std::uint32_t value = 0;
    const auto parsed = std::from_chars(begin, end, value);
    if (parsed.ec == std::errc{} && parsed.ptr == end && value >= min && value <= max) {
        return value;
    }
    return std::nullopt;
}

const char* unitKindName(UnitKind kind) {
    switch (kind) {
    case UnitKind::Light: return "Лёгкий";
    case UnitKind::Heavy: return "Тяжёлый";
    case UnitKind::Archer: return "Лучник";
    case UnitKind::Mage: return "Маг";
    case UnitKind::Healer: return "Целитель";
    }
    return "Юнит";
}

std::string specialActionDescription(UnitKind kind, int power, int range) {
    switch (kind) {
    case UnitKind::Light:
        return "Снаряжение: шанс 25% | тяжёлому соседу | 1 бонус на армию за ход";
    case UnitKind::Heavy: return "Ближний бой";
    case UnitKind::Archer:
        return "Выстрел " + std::to_string(power) + " | дальность " + std::to_string(range);
    case UnitKind::Mage:
        return "Клонирование " + std::to_string(power) + "% | радиус " + std::to_string(range) + " | цели: лёгкие, лучники";
    case UnitKind::Healer:
        return "Лечение " + std::to_string(power) + " HP | радиус " + std::to_string(range) + " | кроме мага";
    }
    return "";
}

std::string counted(std::size_t count, const char* one, const char* few, const char* many) {
    const auto last = count % 10, pair = count % 100;
    return std::to_string(count) + " " + (pair >= 11 && pair <= 14 ? many :
        last == 1 ? one : last >= 2 && last <= 4 ? few : many);
}
std::string unitCount(std::size_t count) { return counted(count,"боец","бойца","бойцов"); }
std::string moneyCount(std::size_t count) { return counted(count,"монета","монеты","монет"); }
