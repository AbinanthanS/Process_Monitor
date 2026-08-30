#include "terminal.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/select.h>
#include <unistd.h>

std::atomic<bool> Terminal::resizedFlag{false};
std::atomic<bool> Terminal::exitRequested{false};

Terminal& Terminal::instance() {
    static Terminal inst;
    return inst;
}

Terminal::Terminal() = default;

Terminal::~Terminal() {
    cleanup();
}

void Terminal::handleResize(int /*sig*/) {
    resizedFlag.store(true);
}

void Terminal::handleSignal(int /*sig*/) {
    exitRequested.store(true);
    // Cleanup will be invoked
    Terminal::instance().cleanup();
    _exit(0);
}

bool Terminal::wasResized() {
    return resizedFlag.load();
}

void Terminal::clearResized() {
    resizedFlag.store(false);
}

bool Terminal::init() {
    if (rawModeActive) return true;

    if (!isatty(STDIN_FILENO)) return false;

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        return false;
    }

    struct sigaction saResize{};
    saResize.sa_handler = Terminal::handleResize;
    sigemptyset(&saResize.sa_mask);
    saResize.sa_flags = 0;
    sigaction(SIGWINCH, &saResize, nullptr);

    struct sigaction saSig{};
    saSig.sa_handler = Terminal::handleSignal;
    sigemptyset(&saSig.sa_mask);
    saSig.sa_flags = 0;
    sigaction(SIGINT, &saSig, nullptr);
    sigaction(SIGTERM, &saSig, nullptr);

    termios raw = orig_termios;
    // Input modes: no break, no CR to NL, no parity check, no strip char, no start/stop output control
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // Output modes: disable post processing
    raw.c_oflag &= ~(OPOST);
    // Control modes: set 8 bit chars
    raw.c_cflag |= (CS8);
    // Local modes: echo off, canonical off, no extended input processing, no signal chars
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    // Control characters: min bytes to read = 0, timeout = 0
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return false;
    }

    rawModeActive = true;

    // Enter alternate screen buffer & hide cursor
    std::cout << "\033[?1049h\033[?25l" << std::flush;
    return true;
}

void Terminal::cleanup() {
    if (rawModeActive) {
        // Exit alternate screen buffer & show cursor
        std::cout << "\033[?25h\033[?1049l" << std::flush;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        rawModeActive = false;
    }
}

TermSize Terminal::getSize() const {
    TermSize ts{24, 80};
    struct winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        ts.rows = ws.ws_row;
        ts.cols = ws.ws_col;
    }
    return ts;
}

KeyEvent Terminal::readKey(int timeoutMs) {
    KeyEvent evt{KeyCode::NONE, 0};

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    struct timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int sel = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
    if (sel <= 0) {
        return evt;
    }

    char c = 0;
    if (read(STDIN_FILENO, &c, 1) <= 0) {
        return evt;
    }

    if (c == 27) { // Escape sequence
        char seq[6] = {0};
        // Check if more characters follow immediately
        struct timeval tvFast{0, 20000}; // 20ms timeout for escape sub-sequence
        fd_set fastFds;
        FD_ZERO(&fastFds);
        FD_SET(STDIN_FILENO, &fastFds);

        if (select(STDIN_FILENO + 1, &fastFds, nullptr, nullptr, &tvFast) > 0) {
            if (read(STDIN_FILENO, &seq[0], 1) > 0) {
                if (seq[0] == '[') {
                    if (read(STDIN_FILENO, &seq[1], 1) > 0) {
                        if (seq[1] >= '0' && seq[1] <= '9') {
                            if (read(STDIN_FILENO, &seq[2], 1) > 0) {
                                if (seq[2] == '~') {
                                    switch (seq[1]) {
                                        case '1': evt.code = KeyCode::HOME; return evt;
                                        case '2': evt.code = KeyCode::NONE; return evt; // Insert
                                        case '3': evt.code = KeyCode::DELETE_KEY; return evt;
                                        case '4': evt.code = KeyCode::END; return evt;
                                        case '5': evt.code = KeyCode::PAGE_UP; return evt;
                                        case '6': evt.code = KeyCode::PAGE_DOWN; return evt;
                                        case '7': evt.code = KeyCode::HOME; return evt;
                                        case '8': evt.code = KeyCode::END; return evt;
                                    }
                                } else if (seq[2] >= '0' && seq[2] <= '9') {
                                    char tilda = 0;
                                    if (read(STDIN_FILENO, &tilda, 1) > 0 && tilda == '~') {
                                        int fnNum = (seq[1] - '0') * 10 + (seq[2] - '0');
                                        switch (fnNum) {
                                            case 15: evt.code = KeyCode::F5; return evt;
                                            case 17: evt.code = KeyCode::F6; return evt;
                                            case 18: evt.code = KeyCode::F7; return evt;
                                            case 19: evt.code = KeyCode::F8; return evt;
                                            case 20: evt.code = KeyCode::F9; return evt;
                                            case 21: evt.code = KeyCode::F10; return evt;
                                            case 23: evt.code = KeyCode::F11; return evt;
                                            case 24: evt.code = KeyCode::F12; return evt;
                                        }
                                    }
                                }
                            }
                        } else {
                            switch (seq[1]) {
                                case 'A': evt.code = KeyCode::UP; return evt;
                                case 'B': evt.code = KeyCode::DOWN; return evt;
                                case 'C': evt.code = KeyCode::RIGHT; return evt;
                                case 'D': evt.code = KeyCode::LEFT; return evt;
                                case 'H': evt.code = KeyCode::HOME; return evt;
                                case 'F': evt.code = KeyCode::END; return evt;
                            }
                        }
                    }
                } else if (seq[0] == 'O') {
                    if (read(STDIN_FILENO, &seq[1], 1) > 0) {
                        switch (seq[1]) {
                            case 'P': evt.code = KeyCode::F1; return evt;
                            case 'Q': evt.code = KeyCode::F2; return evt;
                            case 'R': evt.code = KeyCode::F3; return evt;
                            case 'S': evt.code = KeyCode::F4; return evt;
                            case 'H': evt.code = KeyCode::HOME; return evt;
                            case 'F': evt.code = KeyCode::END; return evt;
                        }
                    }
                }
            }
        }
        evt.code = KeyCode::ESCAPE;
        return evt;
    }

    if (c == 10 || c == 13) {
        evt.code = KeyCode::ENTER;
    } else if (c == 127 || c == 8) {
        evt.code = KeyCode::BACKSPACE;
    } else if (c == 9) {
        evt.code = KeyCode::TAB;
    } else {
        evt.code = KeyCode::CHAR;
        evt.ch = c;
    }

    return evt;
}
