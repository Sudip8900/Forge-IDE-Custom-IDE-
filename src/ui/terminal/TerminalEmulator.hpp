#pragma once

#include "TerminalCell.hpp"
#include <vector>
#include <deque>
#include <string>
#include <mutex>

namespace forge {

class TerminalEmulator {
public:
    TerminalEmulator(int cols, int rows);
    ~TerminalEmulator() = default;

    // Direct interface to consume raw shell stream (UTF-8)
    void write(const std::string& data);
    void resize(int cols, int rows);

    // Thread-safe state query methods
    void lock() { gridMutex.lock(); }
    void unlock() { gridMutex.unlock(); }

    // Read grids (caller should lock/unlock)
    const std::deque<std::vector<TerminalCell>>& getPrimaryGrid() const { return primaryGrid; }
    const std::vector<std::vector<TerminalCell>>& getAltGrid() const { return altGrid; }
    
    int getCursorX() const { return cursorX; }
    int getCursorY() const { return cursorY; }
    int getCols() const { return cols; }
    int getRows() const { return rows; }
    bool isAltBufferActive() const { return altBufferActive; }
    
    // Selection and scrollback index query helpers
    int getScrollbackOffset() const {
        int baseOffset = static_cast<int>(primaryGrid.size()) - rows;
        int offset = baseOffset - scrollOffset;
        return offset >= 0 ? offset : 0;
    }
    void scrollViewport(int lines);
    void clearGrid();

private:
    enum ParserState {
        State_Normal,
        State_Escape,
        State_CSI,
        State_OSC
    };

    void processCodepoint(char32_t cp);
    void handleCSI(char cmd);
    void handleOSC();
    void sgrCommand(const std::vector<int>& params);
    
    void scrollUp(int count = 1);
    void scrollDown(int count = 1);
    void carriageReturn();
    void lineFeed();
    void clearLine(int mode);
    void clearScreen(int mode);
    void eraseCharacters(int count);

    // Grid states
    int cols;
    int rows;
    int cursorX = 0;
    int cursorY = 0;
    int savedCursorX = 0;
    int savedCursorY = 0;

    int scrollTop = 0;
    int scrollBottom = 0;

    bool altBufferActive = false;
    bool lineWrapEnabled = true;

    TerminalCell currentStyle;
    TerminalCell defaultStyle;

    // Buffer grids (deque allows popping old history lines efficiently)
    std::deque<std::vector<TerminalCell>> primaryGrid;
    std::vector<std::vector<TerminalCell>> altGrid;
    
    // VT100 State Machine Parser state
    ParserState parserState = State_Normal;
    std::vector<int> csiParams;
    int currentParam = 0;
    bool hasParam = false;
    std::string oscBuffer;

    // Thread safety mutex
    std::mutex gridMutex;
    
    // Maximum lines of scrollback to preserve
    const size_t maxScrollbackLines = 10000;

    // Viewport scrollback offset (0 = active screen, >0 = scrolled up)
    int scrollOffset = 0;
};

} // namespace forge
