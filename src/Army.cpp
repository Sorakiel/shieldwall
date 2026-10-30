#include "Army.h"
#include <algorithm>

void Army::add(std::unique_ptr<Unit> unit) {
    units_.push_back(std::move(unit));
}

void Army::insert(std::size_t index, std::unique_ptr<Unit> unit) {
    if (index > units_.size()) index = units_.size();
    units_.insert(units_.begin() + static_cast<std::ptrdiff_t>(index), std::move(unit));
}

std::unique_ptr<Unit> Army::replace(std::size_t index, std::unique_ptr<Unit> unit) {
    std::unique_ptr<Unit> old = std::move(units_.at(index));
    units_[index] = std::move(unit);
    return old;
}

Unit* Army::at(std::size_t i) {
    return units_.at(i).get();
}

Unit* Army::front() {
    return units_.empty() ? nullptr : units_.front().get();
}

std::size_t Army::size() const {
    return units_.size();
}

int Army::indexOf(const Unit* u) const {
    for (std::size_t i = 0; i < units_.size(); ++i) {
        if (units_[i].get() == u) return static_cast<int>(i);
    }
    return -1;
}

void Army::removeDead() {
    units_.erase(
        std::remove_if(units_.begin(), units_.end(),
            [](const std::unique_ptr<Unit>& u) { return !u->isAlive(); }),
        units_.end());
}

bool Army::isDefeated() const {
    return std::none_of(units_.begin(), units_.end(),
        [](const std::unique_ptr<Unit>& u) { return u->isAlive(); });
}

int Army::totalCost() const {
    int sum = 0;
    for (const auto& u : units_) sum += u->cost();
    return sum;
}