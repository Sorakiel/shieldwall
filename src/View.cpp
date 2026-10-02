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
    }
    return "Юнит";
}
