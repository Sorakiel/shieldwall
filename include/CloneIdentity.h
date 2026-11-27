#pragma once
#include <charconv>
#include <string>
#include <string_view>

namespace cloneidentity {
struct Name {
    std::string base;
    std::size_t number = 0;
};
inline Name parse(const std::string& name) {
    constexpr std::string_view prefix = " [Клон ";
    const auto start = name.rfind(prefix);
    if (start == std::string::npos || name.empty() || name.back() != ']')
        return {name, 0};
    const auto begin = name.data() + start + prefix.size();
    const auto end = name.data() + name.size() - 1;
    std::size_t number = 0;
    const auto result = std::from_chars(begin, end, number);
    if (result.ec != std::errc{} || result.ptr != end || number == 0)
        return {name, 0};
    return {name.substr(0, start), number};
}
inline std::string numbered(const std::string& source, std::size_t number) {
    return parse(source).base + " [Клон " + std::to_string(number) + "]";
}
} // namespace cloneidentity
