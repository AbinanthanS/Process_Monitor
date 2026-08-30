#ifndef RENDER_BUFFER_H
#define RENDER_BUFFER_H

#include <string>
#include <vector>
#include <cstdint>

struct Rect {
    int y = 1; // 1-indexed row
    int x = 1; // 1-indexed column
    int h = 0; // height
    int w = 0; // width

    bool isValid() const { return h > 1 && w > 2; }
    int innerY() const { return y + 1; }
    int innerX() const { return x + 1; }
    int innerH() const { return h > 2 ? h - 2 : 0; }
    int innerW() const { return w > 2 ? w - 2 : 0; }
};

namespace Color {
    const std::string RESET       = "\033[0m";
    const std::string BOLD        = "\033[1m";
    const std::string DIM         = "\033[2m";
    const std::string ITALIC      = "\033[3m";
    const std::string UNDERLINE   = "\033[4m";
    const std::string INVERT      = "\033[7m";

    // Standard 16 ANSI colors
    const std::string FG_BLACK    = "\033[30m";
    const std::string FG_RED      = "\033[31m";
    const std::string FG_GREEN    = "\033[32m";
    const std::string FG_YELLOW   = "\033[33m";
    const std::string FG_BLUE     = "\033[34m";
    const std::string FG_MAGENTA  = "\033[35m";
    const std::string FG_CYAN     = "\033[36m";
    const std::string FG_WHITE    = "\033[37m";

    const std::string FG_BRIGHT_BLACK   = "\033[90m";
    const std::string FG_BRIGHT_RED     = "\033[91m";
    const std::string FG_BRIGHT_GREEN   = "\033[92m";
    const std::string FG_BRIGHT_YELLOW  = "\033[93m";
    const std::string FG_BRIGHT_BLUE    = "\033[94m";
    const std::string FG_BRIGHT_MAGENTA = "\033[95m";
    const std::string FG_BRIGHT_CYAN    = "\033[96m";
    const std::string FG_BRIGHT_WHITE   = "\033[97m";

    const std::string BG_BLACK    = "\033[40m";
    const std::string BG_RED      = "\033[41m";
    const std::string BG_GREEN    = "\033[42m";
    const std::string BG_YELLOW   = "\033[43m";
    const std::string BG_BLUE     = "\033[44m";
    const std::string BG_MAGENTA  = "\033[45m";
    const std::string BG_CYAN     = "\033[46m";
    const std::string BG_WHITE    = "\033[47m";

    const std::string BG_BRIGHT_BLACK = "\033[100m";
    const std::string BG_BRIGHT_BLUE  = "\033[104m";

    // 24-bit TrueColor generator
    std::string rgb(uint8_t r, uint8_t g, uint8_t b, bool background = false);
    std::string gradient(uint8_t r1, uint8_t g1, uint8_t b1,
                         uint8_t r2, uint8_t g2, uint8_t b2,
                         double factor, bool background = false);
}

class RenderBuffer {
public:
    RenderBuffer(int rows = 24, int cols = 80);

    void resize(int rows, int cols);
    void clear();

    void writeText(int row, int col, const std::string& text, const std::string& style = "");
    void writeTextClipped(int row, int col, int maxLen, const std::string& text, const std::string& style = "");
    void fillRow(int row, char ch = ' ', const std::string& style = "");
    void fillRect(const Rect& rect, char ch = ' ', const std::string& style = "");

    void drawProgressBar(int row, int col, int totalWidth, double percentage, 
                         const std::string& label = "", 
                         const std::string& highColor = Color::FG_BRIGHT_RED,
                         const std::string& midColor = Color::FG_BRIGHT_YELLOW,
                         const std::string& lowColor = Color::FG_BRIGHT_GREEN);

    void drawGradientBar(int row, int col, int totalWidth, double percentage,
                         const std::string& label = "",
                         const std::string& valueSuffix = "%",
                         uint8_t r1 = 0, uint8_t g1 = 200, uint8_t b1 = 255,
                         uint8_t r2 = 255, uint8_t g2 = 60, uint8_t b2 = 120);

    void drawBox(int row, int col, int height, int width, const std::string& title = "", const std::string& style = "");
    void drawRoundedBox(int row, int col, int height, int width,
                        const std::string& title = "",
                        const std::string& badge = "",
                        const std::string& borderStyle = Color::FG_BRIGHT_BLACK,
                        const std::string& titleStyle = Color::BOLD + Color::FG_BRIGHT_CYAN);
    void drawRoundedBox(const Rect& rect,
                        const std::string& title = "",
                        const std::string& badge = "",
                        const std::string& borderStyle = Color::FG_BRIGHT_BLACK,
                        const std::string& titleStyle = Color::BOLD + Color::FG_BRIGHT_CYAN);

    void flush();

    static std::string formatBytes(uint64_t bytes);
    static std::string formatRate(uint64_t bytesPerSec);
    static std::string formatTime(uint64_t totalSeconds);
    static std::string truncateOrPad(const std::string& str, size_t width, bool padRight = true);

    int getRows() const { return rows; }
    int getCols() const { return cols; }

private:
    int rows;
    int cols;
    std::string buffer;
};

#endif // RENDER_BUFFER_H
