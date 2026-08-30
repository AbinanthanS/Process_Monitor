#include "render_buffer.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <unistd.h>

namespace Color {
    std::string rgb(uint8_t r, uint8_t g, uint8_t b, bool background) {
        return "\033[" + std::string(background ? "48;2;" : "38;2;") +
               std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    }
}

RenderBuffer::RenderBuffer(int r, int c) : rows(r), cols(c) {
    buffer.reserve(static_cast<size_t>(rows * (cols + 32)));
}

void RenderBuffer::resize(int r, int c) {
    rows = r;
    cols = c;
    buffer.reserve(static_cast<size_t>(rows * (cols + 32)));
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

void RenderBuffer::fillRow(int row, char ch, const std::string& style) {
    if (row < 1 || row > rows) return;
    buffer += "\033[" + std::to_string(row) + ";1H";
    if (!style.empty()) buffer += style;
    buffer.append(static_cast<size_t>(cols), ch);
    if (!style.empty()) buffer += Color::RESET;
}

void RenderBuffer::drawProgressBar(int row, int col, int totalWidth, double percentage,
                                   const std::string& label,
                                   const std::string& highColor,
                                   const std::string& midColor,
                                   const std::string& lowColor) {
    if (row < 1 || row > rows || col < 1 || col > cols || totalWidth <= 6) return;

    if (percentage < 0.0) percentage = 0.0;
    if (percentage > 100.0) percentage = 100.0;

    std::ostringstream pss;
    pss << std::fixed << std::setprecision(1) << std::setw(5) << percentage << "%";
    std::string pctStr = pss.str();

    int labelLen = static_cast<int>(label.length());
    int overhead = (labelLen > 0 ? labelLen + 1 : 0) + 2 + 1 + static_cast<int>(pctStr.length());
    int innerBarWidth = totalWidth - overhead;
    if (innerBarWidth < 2) innerBarWidth = 2;

    int filledChars = static_cast<int>(std::round((percentage / 100.0) * innerBarWidth));
    if (filledChars > innerBarWidth) filledChars = innerBarWidth;
    if (filledChars < 0) filledChars = 0;
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

void RenderBuffer::drawBox(int row, int col, int height, int width, const std::string& title, const std::string& style) {
    if (row < 1 || col < 1 || height < 2 || width < 2) return;

    std::string top = "┌";
    int innerWidth = width - 2;
    if (!title.empty() && title.length() + 2 <= static_cast<size_t>(innerWidth)) {
        top += " " + title + " ";
        int remaining = innerWidth - static_cast<int>(title.length()) - 2;
        for (int i = 0; i < remaining; ++i) top += "─";
    } else {
        for (int i = 0; i < innerWidth; ++i) top += "─";
    }
    top += "┐";
    writeText(row, col, top, style);

    for (int r = row + 1; r < row + height - 1; ++r) {
        writeText(r, col, "│", style);
        writeText(r, col + width - 1, "│", style);
    }

    std::string bottom = "└";
    for (int i = 0; i < innerWidth; ++i) bottom += "─";
    bottom += "┘";
    writeText(row + height - 1, col, bottom, style);
}

void RenderBuffer::flush() {
    std::cout << buffer << std::flush;
}

std::string RenderBuffer::formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "K", "M", "G", "T", "P"};
    int idx = 0;
    double dBytes = static_cast<double>(bytes);
    while (dBytes >= 1024.0 && idx < 5) {
        dBytes /= 1024.0;
        idx++;
    }
    std::ostringstream ss;
    if (idx == 0) {
        ss << static_cast<uint64_t>(dBytes) << units[idx];
    } else {
        ss << std::fixed << std::setprecision(1) << dBytes << units[idx];
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
