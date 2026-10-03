#pragma once
#include <cstddef>
#include <string>

// Буфер поля SFML: лимит в байтах, удаление по целым кодовым точкам UTF-8.
class TextInput {
public:
    void reset(std::size_t maxBytes) { text_.clear(); maxBytes_ = maxBytes; }
    bool append(char32_t character);
    void backspace();
    void clear() { text_.clear(); }
    const std::string& value() const { return text_; }
private:
    std::string text_;
    std::size_t maxBytes_ = 64;
};
