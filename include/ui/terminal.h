#ifndef TERMINAL_H
#define TERMINAL_H

#include <string>
#include <csignal>
#include <atomic>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

enum class KeyCode {
    NONE = 0,
    CHAR,
    ENTER,
    ESCAPE,
    BACKSPACE,
    TAB,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    PAGE_UP,
    PAGE_DOWN,
    HOME,
    END,
    DELETE_KEY,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12
};

struct KeyEvent {
    KeyCode code = KeyCode::NONE;
    char ch = 0;
};

struct TermSize {
    int rows = 24;
    int cols = 80;
};

class Terminal {
public:
    static Terminal& instance();

    bool init();
    void cleanup();

    TermSize getSize() const;
    KeyEvent readKey(int timeoutMs = 50);

    static void handleResize(int sig);
    static void handleSignal(int sig);
    static bool wasResized();
    static void clearResized();

    ~Terminal();

private:
    Terminal();
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

#if defined(_WIN32)
    DWORD origInMode = 0;
    DWORD origOutMode = 0;
#else
    termios orig_termios{};
#endif
    bool rawModeActive = false;
    static std::atomic<bool> resizedFlag;
    static std::atomic<bool> exitRequested;
};

#endif // TERMINAL_H
