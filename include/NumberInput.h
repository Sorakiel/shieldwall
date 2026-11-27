#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

// Editing of an existing numeric value, independent of window events and fonts.
class NumberInput {
    std::string value_;
    std::size_t cursor_ = 0, anchor_ = 0;
    bool eraseSelection() {
        if (cursor_ == anchor_) return false;
        const auto first = std::min(cursor_, anchor_);
        value_.erase(first, std::max(cursor_, anchor_) - first);
        cursor_ = anchor_ = first;
        return true;
    }
public:
    void reset(std::string value) { value_ = std::move(value); cursor_ = anchor_ = value_.size(); }
    const std::string& value() const { return value_; }
    std::size_t cursor() const { return cursor_; }
    std::size_t selectionStart() const { return std::min(cursor_, anchor_); }
    std::size_t selectionSize() const { return std::max(cursor_, anchor_) - selectionStart(); }
    void selectAll() { anchor_ = 0; cursor_ = value_.size(); }
    void place(std::size_t position, bool extend = false) {
        cursor_ = std::min(position, value_.size());
        if (!extend) anchor_ = cursor_;
    }
    void move(int direction, bool extend = false) {
        if (!extend && selectionSize()) place(direction < 0 ? selectionStart() : selectionStart() + selectionSize());
        else place(direction < 0 ? (cursor_ ? cursor_ - 1 : 0) : cursor_ + 1, extend);
    }
    void insert(char digit) {
        if (digit < '0' || digit > '9' || (value_.size() - selectionSize() >= 20)) return;
        eraseSelection();
        value_.insert(cursor_, 1, digit);
        anchor_ = ++cursor_;
    }
    void backspace() {
        if (!eraseSelection() && cursor_) { value_.erase(--cursor_, 1); anchor_ = cursor_; }
    }
    void eraseForward() { if (!eraseSelection() && cursor_ < value_.size()) value_.erase(cursor_, 1); }
};
