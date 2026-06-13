#include "TerminalEmulator.hpp"
#include <iostream>
#include <algorithm>

namespace forge {

// Helper: Convert UTF-8 bytes to UTF-32 string
static std::u32string utf8_to_utf32(const std::string& str) {
    std::u32string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size();) {
        uint8_t c = str[i];
        if (c < 0x80) {
            result.push_back(c);
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < str.size()) {
                result.push_back(((c & 0x1F) << 6) | (str[i + 1] & 0x3F));
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < str.size()) {
                result.push_back(((c & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F));
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < str.size()) {
                result.push_back(((c & 0x07) << 18) | ((str[i + 1] & 0x3F) << 12) | ((str[i + 2] & 0x3F) << 6) | (str[i + 3] & 0x3F));
            }
            i += 4;
        } else {
            result.push_back(0xFFFD); // Replacement char
            i += 1;
        }
    }
    return result;
}

// Map color index (0-255) to RGBA
static uint32_t getPaletteColor(int index) {
    static const uint32_t ansi16[16] = {
        0x0C0C0CFF, // 0: Black
        0xC50F1FFF, // 1: Red
        0x13A10EFF, // 2: Green
        0xC19C00FF, // 3: Yellow
        0x0037DAFF, // 4: Blue
        0x881798FF, // 5: Magenta
        0x3A96DDFF, // 6: Cyan
        0xCCCCCCFF, // 7: White
        0x767676FF, // 8: Bright Black (Grey)
        0xE74856FF, // 9: Bright Red
        0x16C60CFF, // 10: Bright Green
        0xF9F1A5FF, // 11: Bright Yellow
        0x3B78FFFF, // 12: Bright Blue
        0xB4009EFF, // 13: Bright Magenta
        0x61D6D6FF, // 14: Bright Cyan
        0xF2F2F2FF  // 15: Bright White
    };

    if (index >= 0 && index < 16) {
        return ansi16[index];
    } else if (index >= 16 && index <= 231) {
        int r = ((index - 16) / 36) * 51;
        int g = (((index - 16) / 6) % 6) * 51;
        int b = ((index - 16) % 6) * 51;
        return (r << 24) | (g << 16) | (b << 8) | 0xFF;
    } else if (index >= 232 && index <= 255) {
        int val = (index - 232) * 10 + 8;
        return (val << 24) | (val << 16) | (val << 8) | 0xFF;
    }
    return 0xFFFFFFFF;
}

TerminalEmulator::TerminalEmulator(int cols, int rows) : cols(cols), rows(rows) {
    defaultStyle.fg = 0xCCCCCCFF; // Default grey-white
    defaultStyle.bg = 0x1A1A1AFF; // Slate dark background
    defaultStyle.attributes = 0;
    
    currentStyle = defaultStyle;
    scrollTop = 0;
    scrollBottom = rows - 1;

    clearGrid();
}

void TerminalEmulator::clearGrid() {
    std::lock_guard<std::mutex> lock(gridMutex);
    
    primaryGrid.clear();
    for (int y = 0; y < rows; ++y) {
        primaryGrid.push_back(std::vector<TerminalCell>(cols, defaultStyle));
    }

    altGrid.clear();
    for (int y = 0; y < rows; ++y) {
        altGrid.push_back(std::vector<TerminalCell>(cols, defaultStyle));
    }
    
    cursorX = 0;
    cursorY = 0;
    scrollOffset = 0;
}

void TerminalEmulator::resize(int newCols, int newRows) {
    if (newCols < 1) newCols = 1;
    if (newRows < 1) newRows = 1;
    
    std::lock_guard<std::mutex> lock(gridMutex);
    
    // Resize alt grid
    altGrid.resize(newRows);
    for (int y = 0; y < newRows; ++y) {
        altGrid[y].resize(newCols, defaultStyle);
    }

    // Resize primary grid (maintain existing scrollback size where possible)
    int currentSize = static_cast<int>(primaryGrid.size());
    if (currentSize < newRows) {
        // Grow to reach minimum size
        for (int i = 0; i < (newRows - currentSize); ++i) {
            primaryGrid.push_back(std::vector<TerminalCell>(newCols, defaultStyle));
        }
    } else {
        // Crop excess lines from the bottom if necessary, or just pad/truncate widths of all lines
        for (auto& row : primaryGrid) {
            row.resize(newCols, defaultStyle);
        }
    }

    cols = newCols;
    rows = newRows;
    
    cursorX = std::clamp(cursorX, 0, cols - 1);
    cursorY = std::clamp(cursorY, 0, rows - 1);
    
    scrollTop = 0;
    scrollBottom = rows - 1;
}

void TerminalEmulator::write(const std::string& data) {
    std::lock_guard<std::mutex> lock(gridMutex);
    
    // Auto-scroll to bottom on new output
    scrollOffset = 0;
    
    std::u32string u32data = utf8_to_utf32(data);
    for (char32_t cp : u32data) {
        processCodepoint(cp);
    }
}

void TerminalEmulator::scrollViewport(int lines) {
    std::lock_guard<std::mutex> lock(gridMutex);
    int maxScroll = std::max(0, static_cast<int>(primaryGrid.size()) - rows);
    scrollOffset = std::clamp(scrollOffset + lines, 0, maxScroll);
}

void TerminalEmulator::processCodepoint(char32_t cp) {
    switch (parserState) {
        case State_Normal: {
            if (cp == 0x1b) {
                parserState = State_Escape;
            } else if (cp == '\n') {
                lineFeed();
            } else if (cp == '\r') {
                carriageReturn();
            } else if (cp == '\t') {
                // Advance cursor to next tab stop (multiples of 8)
                int nextX = (cursorX + 8) & ~7;
                cursorX = std::min(nextX, cols - 1);
            } else if (cp == '\b') {
                cursorX = std::max(0, cursorX - 1);
            } else if (cp >= 32) {
                // Handle wrap
                if (cursorX >= cols) {
                    if (lineWrapEnabled) {
                        carriageReturn();
                        lineFeed();
                    } else {
                        cursorX = cols - 1;
                    }
                }
                
                // Write character to current line in appropriate grid
                int activeY = cursorY;
                if (!altBufferActive) {
                    // Offset based on scrollback queue size
                    int scrollOffset = getScrollbackOffset();
                    activeY = std::max(0, scrollOffset) + cursorY;
                    
                    // Double check bounds safety
                    if (activeY >= static_cast<int>(primaryGrid.size())) {
                        primaryGrid.resize(activeY + 1, std::vector<TerminalCell>(cols, defaultStyle));
                    }
                    
                    if (cursorX < cols) {
                        primaryGrid[activeY][cursorX] = currentStyle;
                        primaryGrid[activeY][cursorX].codepoint = cp;
                    }
                } else {
                    if (activeY < static_cast<int>(altGrid.size()) && cursorX < cols) {
                        altGrid[activeY][cursorX] = currentStyle;
                        altGrid[activeY][cursorX].codepoint = cp;
                    }
                }
                
                cursorX++;
            }
            break;
        }
        case State_Escape: {
            if (cp == '[') {
                csiParams.clear();
                currentParam = 0;
                hasParam = false;
                parserState = State_CSI;
            } else if (cp == ']') {
                oscBuffer.clear();
                parserState = State_OSC;
            } else {
                // Single escape commands
                if (cp == 'M') {
                    // Reverse Index (move up, scroll if at top)
                    if (cursorY == scrollTop) {
                        scrollDown();
                    } else {
                        cursorY = std::max(scrollTop, cursorY - 1);
                    }
                } else if (cp == '7') {
                    savedCursorX = cursorX;
                    savedCursorY = cursorY;
                } else if (cp == '8') {
                    cursorX = savedCursorX;
                    cursorY = savedCursorY;
                }
                parserState = State_Normal;
            }
            break;
        }
        case State_CSI: {
            if (cp >= '0' && cp <= '9') {
                currentParam = currentParam * 10 + (cp - '0');
                hasParam = true;
            } else if (cp == ';' || cp == ':') {
                csiParams.push_back(currentParam);
                currentParam = 0;
                hasParam = false;
            } else if (cp == '?') {
                // Mark parameter as query prefix (using negative numbers or flags)
                csiParams.push_back(-1); 
            } else if (cp >= 0x40 && cp <= 0x7E) {
                if (hasParam || csiParams.empty()) {
                    csiParams.push_back(currentParam);
                }
                handleCSI(static_cast<char>(cp));
                parserState = State_Normal;
            }
            break;
        }
        case State_OSC: {
            if (cp == 0x07 || cp == 0x1b) { // BEL or Escape terminates OSC
                handleOSC();
                parserState = State_Normal;
            } else {
                oscBuffer.push_back(static_cast<char>(cp));
            }
            break;
        }
    }
}

void TerminalEmulator::handleCSI(char cmd) {
    // Helper to fetch CSI parameter with default fallbacks
    auto getParam = [](const std::vector<int>& params, size_t index, int fallback) -> int {
        if (index < params.size() && params[index] >= 0) {
            return params[index];
        }
        return fallback;
    };

    switch (cmd) {
        case 'm': // SGR (Styling)
            sgrCommand(csiParams);
            break;
        case 'H': // Cursor Position
        case 'f': {
            int line = getParam(csiParams, 0, 1);
            int col = getParam(csiParams, 1, 1);
            cursorY = std::clamp(line - 1, 0, rows - 1);
            cursorX = std::clamp(col - 1, 0, cols - 1);
            break;
        }
        case 'A': { // Cursor Up
            int count = getParam(csiParams, 0, 1);
            cursorY = std::max(scrollTop, cursorY - count);
            break;
        }
        case 'B': { // Cursor Down
            int count = getParam(csiParams, 0, 1);
            cursorY = std::min(scrollBottom, cursorY + count);
            break;
        }
        case 'C': { // Cursor Forward
            int count = getParam(csiParams, 0, 1);
            cursorX = std::min(cols - 1, cursorX + count);
            break;
        }
        case 'D': { // Cursor Backward
            int count = getParam(csiParams, 0, 1);
            cursorX = std::max(0, cursorX - count);
            break;
        }
        case 'G': { // Cursor Horizontal Absolute
            int col = getParam(csiParams, 0, 1);
            cursorX = std::clamp(col - 1, 0, cols - 1);
            break;
        }
        case 'J': { // Erase in Display
            int mode = getParam(csiParams, 0, 0);
            clearScreen(mode);
            break;
        }
        case 'K': { // Erase in Line
            int mode = getParam(csiParams, 0, 0);
            clearLine(mode);
            break;
        }
        case 'r': { // Set Scrolling Region
            int top = getParam(csiParams, 0, 1);
            int bottom = getParam(csiParams, 1, rows);
            scrollTop = std::clamp(top - 1, 0, rows - 1);
            scrollBottom = std::clamp(bottom - 1, 0, rows - 1);
            break;
        }
        case 'X': { // Erase Characters
            int count = getParam(csiParams, 0, 1);
            eraseCharacters(count);
            break;
        }
        case 'h': // Set Mode
        case 'l': { // Reset Mode
            bool set = (cmd == 'h');
            if (!csiParams.empty() && csiParams[0] == -1) { // "? Prefix"
                int mode = getParam(csiParams, 1, 0);
                if (mode == 1049) {
                    altBufferActive = set;
                    cursorX = 0;
                    cursorY = 0;
                    if (altBufferActive) {
                        // Clear alternate screen buffer
                        for (auto& row : altGrid) {
                            std::fill(row.begin(), row.end(), defaultStyle);
                        }
                    }
                } else if (mode == 7) {
                    lineWrapEnabled = set;
                }
            }
            break;
        }
    }
}

void TerminalEmulator::sgrCommand(const std::vector<int>& params) {
    if (params.empty()) {
        currentStyle = defaultStyle;
        return;
    }

    for (size_t i = 0; i < params.size(); ++i) {
        int val = params[i];
        if (val == 0) {
            currentStyle = defaultStyle;
        } else if (val == 1) {
            currentStyle.attributes |= TerminalCell::Attr_Bold;
        } else if (val == 3) {
            currentStyle.attributes |= TerminalCell::Attr_Italic;
        } else if (val == 4) {
            currentStyle.attributes |= TerminalCell::Attr_Underline;
        } else if (val == 5) {
            currentStyle.attributes |= TerminalCell::Attr_Blink;
        } else if (val == 7) {
            currentStyle.attributes |= TerminalCell::Attr_Inverted;
        } else if (val == 22) {
            currentStyle.attributes &= ~TerminalCell::Attr_Bold;
        } else if (val == 23) {
            currentStyle.attributes &= ~TerminalCell::Attr_Italic;
        } else if (val == 24) {
            currentStyle.attributes &= ~TerminalCell::Attr_Underline;
        } else if (val == 27) {
            currentStyle.attributes &= ~TerminalCell::Attr_Inverted;
        } else if (val >= 30 && val <= 37) {
            currentStyle.fg = getPaletteColor(val - 30);
        } else if (val == 38) { // Foreground truecolor or 256
            if (i + 1 < params.size()) {
                int mode = params[i + 1];
                if (mode == 5 && i + 2 < params.size()) {
                    currentStyle.fg = getPaletteColor(params[i + 2]);
                    i += 2;
                } else if (mode == 2 && i + 4 < params.size()) {
                    int r = params[i + 2];
                    int g = params[i + 3];
                    int b = params[i + 4];
                    currentStyle.fg = (r << 24) | (g << 16) | (b << 8) | 0xFF;
                    i += 4;
                }
            }
        } else if (val == 39) {
            currentStyle.fg = defaultStyle.fg;
        } else if (val >= 40 && val <= 47) {
            currentStyle.bg = getPaletteColor(val - 40);
        } else if (val == 48) { // Background truecolor or 256
            if (i + 1 < params.size()) {
                int mode = params[i + 1];
                if (mode == 5 && i + 2 < params.size()) {
                    currentStyle.bg = getPaletteColor(params[i + 2]);
                    i += 2;
                } else if (mode == 2 && i + 4 < params.size()) {
                    int r = params[i + 2];
                    int g = params[i + 3];
                    int b = params[i + 4];
                    currentStyle.bg = (r << 24) | (g << 16) | (b << 8) | 0xFF;
                    i += 4;
                }
            }
        } else if (val == 49) {
            currentStyle.bg = defaultStyle.bg;
        } else if (val >= 90 && val <= 97) {
            currentStyle.fg = getPaletteColor(val - 90 + 8); // High intensity fg
        } else if (val >= 100 && val <= 107) {
            currentStyle.bg = getPaletteColor(val - 100 + 8); // High intensity bg
        }
    }
}

void TerminalEmulator::handleOSC() {
    // OSC commands normally set window titles. OSC 0/2: Set window title
    // Format: \x1b]0;Title\x07
    if ((oscBuffer.rfind("0;", 0) == 0 || oscBuffer.rfind("2;", 0) == 0) && oscBuffer.length() > 2) {
        std::string title = oscBuffer.substr(2);
        // We can record this or update shell session titles later
    }
}

void TerminalEmulator::scrollUp(int count) {
    if (altBufferActive) {
        // In Alt Buffer, scroll current viewport region
        for (int c = 0; c < count; ++c) {
            for (int y = scrollTop; y < scrollBottom; ++y) {
                altGrid[y] = altGrid[y + 1];
            }
            altGrid[scrollBottom] = std::vector<TerminalCell>(cols, defaultStyle);
        }
    } else {
        // In Primary Buffer, shift lines up by appending new lines to the deque
        for (int c = 0; c < count; ++c) {
            primaryGrid.push_back(std::vector<TerminalCell>(cols, defaultStyle));
            
            // Apply maximum scrollback truncation
            if (primaryGrid.size() > maxScrollbackLines) {
                primaryGrid.pop_front();
            }
        }
    }
}

void TerminalEmulator::scrollDown(int count) {
    if (altBufferActive) {
        for (int c = 0; c < count; ++c) {
            for (int y = scrollBottom; y > scrollTop; --y) {
                altGrid[y] = altGrid[y - 1];
            }
            altGrid[scrollTop] = std::vector<TerminalCell>(cols, defaultStyle);
        }
    } else {
        // Shift scrollback downwards
        for (int c = 0; c < count; ++c) {
            primaryGrid.push_front(std::vector<TerminalCell>(cols, defaultStyle));
            if (primaryGrid.size() > maxScrollbackLines) {
                primaryGrid.pop_back();
            }
        }
    }
}

void TerminalEmulator::carriageReturn() {
    cursorX = 0;
}

void TerminalEmulator::lineFeed() {
    if (cursorY == scrollBottom) {
        scrollUp();
    } else {
        cursorY = std::min(rows - 1, cursorY + 1);
    }
}

void TerminalEmulator::clearLine(int mode) {
    int activeY = cursorY;
    if (!altBufferActive) {
        activeY = getScrollbackOffset() + cursorY;
    }

    if (mode == 0) { // Erase from cursor to end of line
        if (altBufferActive) {
            std::fill(altGrid[activeY].begin() + cursorX, altGrid[activeY].end(), defaultStyle);
        } else {
            if (activeY >= 0 && activeY < static_cast<int>(primaryGrid.size())) {
                std::fill(primaryGrid[activeY].begin() + cursorX, primaryGrid[activeY].end(), defaultStyle);
            }
        }
    } else if (mode == 1) { // Erase from beginning of line to cursor
        if (altBufferActive) {
            std::fill(altGrid[activeY].begin(), altGrid[activeY].begin() + std::min(cursorX + 1, cols), defaultStyle);
        } else {
            if (activeY >= 0 && activeY < static_cast<int>(primaryGrid.size())) {
                std::fill(primaryGrid[activeY].begin(), primaryGrid[activeY].begin() + std::min(cursorX + 1, cols), defaultStyle);
            }
        }
    } else if (mode == 2) { // Erase entire line
        if (altBufferActive) {
            std::fill(altGrid[activeY].begin(), altGrid[activeY].end(), defaultStyle);
        } else {
            if (activeY >= 0 && activeY < static_cast<int>(primaryGrid.size())) {
                std::fill(primaryGrid[activeY].begin(), primaryGrid[activeY].end(), defaultStyle);
            }
        }
    }
}

void TerminalEmulator::clearScreen(int mode) {
    if (mode == 0) { // Erase from cursor to end of screen
        clearLine(0);
        for (int y = cursorY + 1; y < rows; ++y) {
            int activeY = altBufferActive ? y : (getScrollbackOffset() + y);
            if (altBufferActive) {
                std::fill(altGrid[activeY].begin(), altGrid[activeY].end(), defaultStyle);
            } else {
                if (activeY >= 0 && activeY < static_cast<int>(primaryGrid.size())) {
                    std::fill(primaryGrid[activeY].begin(), primaryGrid[activeY].end(), defaultStyle);
                }
            }
        }
    } else if (mode == 1) { // Erase from beginning of screen to cursor
        clearLine(1);
        for (int y = 0; y < cursorY; ++y) {
            int activeY = altBufferActive ? y : (getScrollbackOffset() + y);
            if (altBufferActive) {
                std::fill(altGrid[activeY].begin(), altGrid[activeY].end(), defaultStyle);
            } else {
                if (activeY >= 0 && activeY < static_cast<int>(primaryGrid.size())) {
                    std::fill(primaryGrid[activeY].begin(), primaryGrid[activeY].end(), defaultStyle);
                }
            }
        }
    } else if (mode == 2) { // Erase entire screen and scrollback
        if (altBufferActive) {
            for (auto& row : altGrid) {
                std::fill(row.begin(), row.end(), defaultStyle);
            }
        } else {
            primaryGrid.clear();
            for (int y = 0; y < rows; ++y) {
                primaryGrid.push_back(std::vector<TerminalCell>(cols, defaultStyle));
            }
        }
        cursorX = 0;
        cursorY = 0;
    }
}

void TerminalEmulator::eraseCharacters(int count) {
    int activeY = altBufferActive ? cursorY : (getScrollbackOffset() + cursorY);
    int endX = std::min(cursorX + count, cols);
    if (altBufferActive) {
        std::fill(altGrid[activeY].begin() + cursorX, altGrid[activeY].begin() + endX, defaultStyle);
    } else {
        if (activeY >= 0 && activeY < static_cast<int>(primaryGrid.size())) {
            std::fill(primaryGrid[activeY].begin() + cursorX, primaryGrid[activeY].begin() + endX, defaultStyle);
        }
    }
}

} // namespace forge
