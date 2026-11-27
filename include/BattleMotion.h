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
    Scroll(float start, float end, float seconds = 0) : from(start), to(end) {
        duration = seconds > 0 ? seconds : .20f + std::min(.30f, std::sqrt(std::abs(to - from)) * .055f);
    }
    float value() const { return from + (to - from) * smooth(elapsed / duration); }
    bool advance(float seconds) {
        if (std::isfinite(seconds) && seconds > 0) elapsed = std::min(duration, elapsed + seconds);
        return elapsed >= duration;
    }
};
inline float maxOffset(std::size_t count, std::size_t visible) {
    return static_cast<float>(count > visible ? count - visible : 0);
}
inline float wheelOffset(float target, float delta, std::size_t count, std::size_t visible) {
    if (!std::isfinite(delta)) return target;
    return std::clamp(target - delta * 2.f, 0.f, maxOffset(count, visible));
}
inline float focusOffset(float current, int low, int high, std::size_t count, std::size_t visible) {
    if (low < 0 || high < low || visible == 0) return current;
    float result = current;
    if (low < current) result = static_cast<float>(low);
    else if (high >= current + visible) result = static_cast<float>(high) - visible + 1;
    return std::clamp(result, 0.f, maxOffset(count, visible));
}
inline int dragScrollDirection(float centerX, float centerY) {
    if (centerY < 570 || centerY > 744) return 0;
    return centerX < 160 ? -1 : centerX > 1270 ? 1 : 0;
}
inline float edgeScrollOffset(float current, int direction, float seconds, std::size_t count,
                              std::size_t visible) {
    if (!std::isfinite(seconds) || seconds <= 0) return current;
    // Continuous motion while held at an edge: two cards per second.
    return std::clamp(current + direction * seconds * 2.f, 0.f, maxOffset(count, visible));
}
} // namespace presentation
