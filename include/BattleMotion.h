#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace presentation {
inline constexpr std::array<float, 4> Speeds{.5f, 1.f, 2.f, 4.f};
// New 1x is the former 0.5x. Focus never controls the battle clock.
inline float playbackSeconds(float seconds, float speed, bool paused = false) {
    if (paused || !std::isfinite(seconds) || seconds < 0 || !std::isfinite(speed) || speed <= 0)
        return 0;
    return seconds * speed * .5f;
}
inline float smooth(float t) {
    t = std::clamp(t, 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
struct Scroll {
    float from = 0, to = 0, elapsed = 0;
    float duration = .5f;
    Scroll() = default;
    Scroll(float start, float end) : from(start), to(end) {
        duration = .28f + std::min(.55f, std::sqrt(std::abs(to - from)) * .08f);
    }
    float value() const { return from + (to - from) * smooth(elapsed / duration); }
    bool advance(float seconds) {
        if (std::isfinite(seconds) && seconds > 0) elapsed = std::min(duration, elapsed + seconds);
        return elapsed >= duration;
    }
};
inline std::size_t focusOffset(std::size_t current, int low, int high, std::size_t count,
                               std::size_t page) {
    if (low < 0 || high < low || page == 0) return current;
    auto result = current;
    if (static_cast<std::size_t>(low) < current) result = static_cast<std::size_t>(low);
    else if (static_cast<std::size_t>(high) >= current + page)
        result = static_cast<std::size_t>(high) - page + 1;
    return std::min(result, count > page ? count - page : 0);
}
inline int dragPageDirection(float centerX, float centerY) {
    if (centerY < 570 || centerY > 744) return 0;
    return centerX < 135 ? -1 : centerX > 1210 ? 1 : 0;
}
class DragPager {
    int direction_ = 0;
    float held_ = 0;
public:
    void reset() { direction_ = 0; held_ = 0; }
    int advance(int direction, float seconds, bool previous, bool next) {
        if ((direction < 0 && !previous) || (direction > 0 && !next)) direction = 0;
        if (direction != direction_) { direction_ = direction; held_ = 0; }
        if (direction == 0) { held_ = 0; return 0; }
        if (std::isfinite(seconds) && seconds > 0) held_ += seconds;
        if (held_ < .55f) return 0;
        held_ = 0;
        return direction;
    }
};
} // namespace presentation
