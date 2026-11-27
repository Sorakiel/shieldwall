#pragma once
#include <string>
#include <type_traits>
#include <utility>

namespace presentation {
struct CloneData {
    int sourceIndex = -1;
    std::string name;
    int hp = -1;
};
// Accept the fields requested in PR 29 when the core supplies them. The existing
// shared BattleEvent remains untouched, and old events retain a safe fallback.
template<class Event, class = void> struct CloneFields {
    static CloneData read(const Event&) { return {}; }
};
template<class Event> struct CloneFields<Event, std::void_t<
    decltype(std::declval<Event>().sourceIndex), decltype(std::declval<Event>().cloneName),
    decltype(std::declval<Event>().cloneHp)>> {
    static CloneData read(const Event& event) { return {event.sourceIndex, event.cloneName, event.cloneHp}; }
};
template<class Event> CloneData cloneData(const Event& event) { return CloneFields<Event>::read(event); }
} // namespace presentation
