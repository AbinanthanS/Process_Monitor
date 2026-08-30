#ifndef THEME_H
#define THEME_H

#include "ui/render_buffer.h"
#include <string>
#include <vector>

enum class ThemePreset {
    TOKYO_NIGHT = 0,
    DRACULA,
    NORD,
    CYBERPUNK,
    MONOKAI,
    MATRIX,
    COUNT
};

struct ThemeColor {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;

    std::string fg() const { return Color::rgb(r, g, b, false); }
    std::string bg() const { return Color::rgb(r, g, b, true); }
};

struct Theme {
    std::string name;
    ThemePreset preset;

    // Base background & frame
    ThemeColor bg;
    ThemeColor frameBorder;
    ThemeColor textMain;
    ThemeColor textDim;
    ThemeColor titleActive;

    // Panel Header Accent Colors
    ThemeColor cpuBorder;
    ThemeColor memBorder;
    ThemeColor diskBorder;
    ThemeColor netBorder;
    ThemeColor procBorder;

    // Meter Gradients (Start -> End)
    ThemeColor meterLow;
    ThemeColor meterMid;
    ThemeColor meterHigh;

    // Selection
    ThemeColor selBg;
    ThemeColor selFg;

    // Graphs
    ThemeColor graphCpu;
    ThemeColor graphMem;
    ThemeColor graphNetRx;
    ThemeColor graphNetTx;
    ThemeColor graphDiskRead;
    ThemeColor graphDiskWrite;
};

class ThemeManager {
public:
    static ThemeManager& instance();

    const Theme& current() const { return themes[currentIdx]; }
    ThemePreset getPreset() const { return static_cast<ThemePreset>(currentIdx); }
    void setPreset(ThemePreset preset);
    void nextTheme();

    const std::vector<Theme>& getAllThemes() const { return themes; }

private:
    ThemeManager();
    std::vector<Theme> themes;
    size_t currentIdx = 0;
};

#endif // THEME_H
