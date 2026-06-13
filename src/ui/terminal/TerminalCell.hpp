#pragma once

#include <cstdint>

namespace forge {

struct TerminalCell {
    char32_t codepoint = ' ';
    uint32_t fg = 0xFFFFFFFF; // White (RGBA: 255, 255, 255, 255)
    uint32_t bg = 0x00000000; // Transparent/Black (RGBA: 0, 0, 0, 0)
    uint8_t attributes = 0;   // Bitmask: Bold=1, Underline=2, Italic=4, Blink=8, Inverted=16

    enum AttributeMask : uint8_t {
        Attr_None      = 0,
        Attr_Bold      = 1 << 0,
        Attr_Underline = 1 << 1,
        Attr_Italic    = 1 << 2,
        Attr_Blink     = 1 << 3,
        Attr_Inverted  = 1 << 4
    };
};

} // namespace forge
