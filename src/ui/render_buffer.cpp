#include "ui/render_buffer.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

#if !defined(_WIN32)
#include <unistd.h>
#endif

static std::string repeatUtf8(const std::string& str, int count) {
    if (count <= 0) return "";
    std::string res;
    res.reserve(str.length() * static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        res += str;
    }
    return res;
}

namespace Color {
    std::string rgb(uint8_t r, uint8_t g, uint8_t b, bool background) {
        return "\033[" + std::string(background ? "48;2;" : "38;2;") +
               std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    }

    std::string gradient(uint8_t r1, uint8_t g1, uint8_t b1,
                         uint8_t r2, uint8_t g2, uint8_t b2,
                         double factor, bool background) {
        factor = std::clamp(factor, 0.0, 1.0);
        uint8_t r = static_cast<uint8_t>(r1 + (r2 - r1) * factor);
        uint8_t g = static_cast<uint8_t>(g1 + (g2 - g1) * factor);
        uint8_t b = static_cast<uint8_t>(b1 + (b2 - b1) * factor);
        return rgb(r, g, b, background);
    }
}

RenderBuffer::RenderBuffer(int r, int c) : rows(r), cols(c) {
    buffer.reserve(static_cast<size_t>(rows * (cols + 64)));
}

void RenderBuffer::resize(int r, int c) {
    rows = r;
    cols = c;
    buffer.reserve(static_cast<size_t>(rows * (cols + 64)));
}

void RenderBuffer::clear() {
    buffer.clear();
    buffer += "\033[H";
}

void RenderBuffer::writeText(int row, int col, const std::string& text, const std::string& style) {
    if (row < 1 || row > rows || col < 1 || col > cols) return;
    buffer += "\033[" + std::to_string(row) + ";" + std::to_string(col) + "H";
    if (!style.empty()) buffer += style;
    buffer += text;
    if (!style.empty()) buffer += Color::RESET;
}

void RenderBuffer::writeTextClipped(int row, int col, int maxLen, const std::string& text, const std::string& style) {
    if (row < 1 || row > rows || col < 1 || col > cols || maxLen <= 0) return;
    std::string clipped = truncateOrPad(text, static_cast<size_t>(maxLen), false);
    writeText(row, col, clipped, style);
}

void RenderBuffer::fillRow(int row, char ch, const std::string& style) {
    if (row < 1 || row > rows) return;
    buffer += "\033[" + std::to_string(row) + ";1H";
    if (!style.empty()) buffer += style;
    buffer.append(static_cast<size_t>(cols), ch);
    if (!style.empty()) buffer += Color::RESET;
}

void RenderBuffer::fillRect(const Rect& rect, char ch, const std::string& style) {
    if (!rect.isValid()) return;
    for (int r = rect.y; r < rect.y + rect.h; ++r) {
        if (r < 1 || r > rows) continue;
        int startCol = std::max(1, rect.x);
        int endCol = std::min(cols, rect.x + rect.w - 1);
        int count = endCol - startCol + 1;
        if (count <= 0) continue;

        buffer += "\033[" + std::to_string(r) + ";" + std::to_string(startCol) + "H";
        if (!style.empty()) buffer += style;
        buffer.append(static_cast<size_t>(count), ch);
        if (!style.empty()) buffer += Color::RESET;
    }
}

void RenderBuffer::drawProgressBar(int row, int col, int totalWidth, double percentage,
                                   const std::string& label,
                                   const std::string& highColor,
                                   const std::string& midColor,
                                   const std::string& lowColor) {
    if (row < 1 || row > rows || col < 1 || col > cols || totalWidth <= 6) return;

    percentage = std::clamp(percentage, 0.0, 100.0);

    std::ostringstream pss;
    pss << std::fixed << std::setprecision(1) << std::setw(5) << percentage << "%";
    std::string pctStr = pss.str();

    int labelLen = static_cast<int>(label.length());
    int overhead = (labelLen > 0 ? labelLen + 1 : 0) + 2 + 1 + static_cast<int>(pctStr.length());
    int innerBarWidth = totalWidth - overhead;
    if (innerBarWidth < 2) innerBarWidth = 2;

    int filledChars = static_cast<int>(std::round((percentage / 100.0) * innerBarWidth));
    filledChars = std::clamp(filledChars, 0, innerBarWidth);
    int emptyChars = innerBarWidth - filledChars;

    std::string color = lowColor;
    if (percentage >= 80.0) {
        color = highColor;
    } else if (percentage >= 50.0) {
        color = midColor;
    }

    std::ostringstream ss;
    if (labelLen > 0) {
        ss << Color::BOLD << label << Color::RESET << " ";
    }
    ss << Color::FG_BRIGHT_BLACK << "[" << Color::RESET;
    ss << color << std::string(filledChars, '|') << Color::RESET;
    if (emptyChars > 0) {
        ss << std::string(emptyChars, ' ');
    }
    ss << Color::FG_BRIGHT_BLACK << "]" << Color::RESET;
    ss << " " << Color::BOLD << pctStr << Color::RESET;

    writeText(row, col, ss.str());
}

void RenderBuffer::drawGradientBar(int row, int col, int totalWidth, double percentage,
                                   const std::string& label,
                                   const std::string& valueSuffix,
                                   uint8_t r1, uint8_t g1, uint8_t b1,
                                   uint8_t r2, uint8_t g2, uint8_t b2) {
    if (row < 1 || row > rows || col < 1 || col > cols || totalWidth <= 6) return;

    percentage = std::clamp(percentage, 0.0, 100.0);

    std::ostringstream pss;
    pss << std::fixed << std::setprecision(1) << std::setw(5) << percentage << valueSuffix;
    std::string pctStr = pss.str();

    int labelLen = static_cast<int>(label.length());
    int overhead = (labelLen > 0 ? labelLen + 1 : 0) + 1 + static_cast<int>(pctStr.length());
    int barWidth = totalWidth - overhead;
    if (barWidth < 2) barWidth = 2;

    std::ostringstream ss;
    if (labelLen > 0) {
        ss << Color::BOLD << label << Color::RESET << " ";
    }

    double fillExact = (percentage / 100.0) * barWidth;
    int fullBlocks = static_cast<int>(fillExact);
    double remainder = fillExact - fullBlocks;

    for (int i = 0; i < fullBlocks; ++i) {
        double factor = static_cast<double>(i) / std::max(1, barWidth - 1);
        std::string colStr = Color::gradient(r1, g1, b1, r2, g2, b2, factor);
        ss << colStr << "■";
    }

    if (fullBlocks < barWidth) {
        if (remainder > 0.3) {
            double factor = static_cast<double>(fullBlocks) / std::max(1, barWidth - 1);
            std::string colStr = Color::gradient(r1, g1, b1, r2, g2, b2, factor);
            ss << colStr << "▪" << Color::RESET;
            fullBlocks++;
        }
        int remaining = barWidth - fullBlocks;
        if (remaining > 0) {
            ss << Color::FG_BRIGHT_BLACK << repeatUtf8("·", remaining) << Color::RESET;
        }
    }
    ss << Color::RESET << " " << Color::BOLD << pctStr << Color::RESET;

    writeText(row, col, ss.str());
}

void RenderBuffer::drawBox(int row, int col, int height, int width, const std::string& title, const std::string& style) {
    if (row < 1 || col < 1 || height < 2 || width < 2) return;

    std::string top = "┌";
    int innerWidth = width - 2;
    if (!title.empty() && title.length() + 2 <= static_cast<size_t>(innerWidth)) {
        top += " " + title + " ";
        int remaining = innerWidth - static_cast<int>(title.length()) - 2;
        top += repeatUtf8("─", remaining);
    } else {
        top += repeatUtf8("─", innerWidth);
    }
    top += "┐";
    writeText(row, col, top, style);

    for (int r = row + 1; r < row + height - 1; ++r) {
        writeText(r, col, "│", style);
        writeText(r, col + width - 1, "│", style);
    }

    std::string bottom = "└" + repeatUtf8("─", innerWidth) + "┘";
    writeText(row + height - 1, col, bottom, style);
}

void RenderBuffer::drawRoundedBox(int row, int col, int height, int width,
                                  const std::string& title,
                                  const std::string& badge,
                                  const std::string& borderStyle,
                                  const std::string& titleStyle) {
    if (row < 1 || col < 1 || height < 2 || width < 2) return;

    std::ostringstream topSS;
    topSS << borderStyle << "╭─" << Color::RESET;

    int usedLen = 2;

    if (!title.empty()) {
        topSS << " " << titleStyle << title << Color::RESET << " ";
        usedLen += static_cast<int>(title.length()) + 2;
    }

    int badgeLen = static_cast<int>(badge.length());
    int remaining = (width - 1) - usedLen - (badgeLen > 0 ? (badgeLen + 2) : 0);

    if (remaining < 0) remaining = 0;
    topSS << borderStyle << repeatUtf8("─", remaining) << Color::RESET;

    if (badgeLen > 0 && usedLen + remaining + badgeLen + 2 < width) {
        topSS << " " << Color::FG_BRIGHT_WHITE << Color::BOLD << badge << Color::RESET << " ";
    }

    topSS << borderStyle << "╮" << Color::RESET;
    writeText(row, col, topSS.str());

    for (int r = row + 1; r < row + height - 1; ++r) {
        writeText(r, col, "│", borderStyle);
        writeText(r, col + width - 1, "│", borderStyle);
    }

    int innerWidth = width - 2;
    std::string bottom = "╰" + repeatUtf8("─", innerWidth) + "╯";
    writeText(row + height - 1, col, bottom, borderStyle);
}

void RenderBuffer::drawRoundedBox(const Rect& rect,
                                  const std::string& title,
                                  const std::string& badge,
                                  const std::string& borderStyle,
                                  const std::string& titleStyle) {
    drawRoundedBox(rect.y, rect.x, rect.h, rect.w, title, badge, borderStyle, titleStyle);
}

void RenderBuffer::flush() {
    std::cout << buffer << std::flush;
}

std::string RenderBuffer::formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    int idx = 0;
    double dBytes = static_cast<double>(bytes);
    while (dBytes >= 1024.0 && idx < 5) {
        dBytes /= 1024.0;
        idx++;
    }
    std::ostringstream ss;
    if (idx == 0) {
        ss << static_cast<uint64_t>(dBytes) << " " << units[idx];
    } else {
        ss << std::fixed << std::setprecision(1) << dBytes << " " << units[idx];
    }
    return ss.str();
}

std::string RenderBuffer::formatRate(uint64_t bytesPerSec) {
    const char* units[] = {"B/s", "KB/s", "MB/s", "GB/s", "TB/s"};
    int idx = 0;
    double dRate = static_cast<double>(bytesPerSec);
    while (dRate >= 1024.0 && idx < 4) {
        dRate /= 1024.0;
        idx++;
    }
    std::ostringstream ss;
    if (idx == 0) {
        ss << static_cast<uint64_t>(dRate) << " " << units[idx];
    } else {
        ss << std::fixed << std::setprecision(1) << dRate << " " << units[idx];
    }
    return ss.str();
}

std::string RenderBuffer::formatTime(uint64_t totalSeconds) {
    uint64_t hours = totalSeconds / 3600;
    uint64_t minutes = (totalSeconds % 3600) / 60;
    uint64_t seconds = totalSeconds % 60;

    std::ostringstream ss;
    if (hours > 0) {
        ss << hours << "h" << std::setw(2) << std::setfill('0') << minutes << "m";
    } else {
        ss << minutes << ":" << std::setw(2) << std::setfill('0') << seconds;
    }
    return ss.str();
}

std::string RenderBuffer::truncateOrPad(const std::string& str, size_t width, bool padRight) {
    if (str.length() > width) {
        if (width <= 3) return str.substr(0, width);
        return str.substr(0, width - 1) + "~";
    }
    if (str.length() == width) return str;

    size_t padCount = width - str.length();
    if (padRight) {
        return str + std::string(padCount, ' ');
    } else {
        return std::string(padCount, ' ') + str;
    }
}
