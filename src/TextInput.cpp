#include "TextInput.h"

bool TextInput::append(char32_t character) {
    if (character < 0x20 || (character >= 0x7f && character <= 0x9f) ||
        (character >= 0xd800 && character <= 0xdfff) || character > 0x10ffff) return false;
    std::string encoded;
    if (character <= 0x7f) encoded += static_cast<char>(character);
    else if (character <= 0x7ff) {
        encoded += static_cast<char>(0xc0 | (character >> 6));
        encoded += static_cast<char>(0x80 | (character & 0x3f));
    } else if (character <= 0xffff) {
        encoded += static_cast<char>(0xe0 | (character >> 12));
        encoded += static_cast<char>(0x80 | ((character >> 6) & 0x3f));
        encoded += static_cast<char>(0x80 | (character & 0x3f));
    } else {
        encoded += static_cast<char>(0xf0 | (character >> 18));
        encoded += static_cast<char>(0x80 | ((character >> 12) & 0x3f));
        encoded += static_cast<char>(0x80 | ((character >> 6) & 0x3f));
        encoded += static_cast<char>(0x80 | (character & 0x3f));
    }
    if (encoded.size() > maxBytes_ - text_.size()) return false;
    text_ += encoded;
    return true;
}

void TextInput::backspace() {
    if (text_.empty()) return;
    std::size_t position = text_.size() - 1;
    while (position > 0 && (static_cast<unsigned char>(text_[position]) & 0xc0) == 0x80) --position;
    text_.erase(position);
}
