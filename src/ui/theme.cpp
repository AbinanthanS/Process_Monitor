#include "ui/theme.h"

ThemeManager& ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

ThemeManager::ThemeManager() {
    // 1. Tokyo Night (Deep navy, neon cyan, soft violet accents)
    Theme tokyo;
    tokyo.name = "Tokyo Night";
    tokyo.preset = ThemePreset::TOKYO_NIGHT;
    tokyo.bg = {26, 27, 38};
    tokyo.frameBorder = {65, 72, 104};
    tokyo.textMain = {192, 202, 245};
    tokyo.textDim = {86, 95, 137};
    tokyo.titleActive = {122, 162, 247};
    tokyo.cpuBorder = {122, 162, 247};   // Neon Blue
    tokyo.memBorder = {187, 154, 247};   // Purple
    tokyo.diskBorder = {224, 175, 104};  // Amber
    tokyo.netBorder = {158, 206, 106};   // Green
    tokyo.procBorder = {125, 207, 255};  // Cyan
    tokyo.meterLow = {158, 206, 106};
    tokyo.meterMid = {224, 175, 104};
    tokyo.meterHigh = {247, 118, 142};
    tokyo.selBg = {54, 75, 126};
    tokyo.selFg = {255, 255, 255};
    tokyo.graphCpu = {122, 162, 247};
    tokyo.graphMem = {187, 154, 247};
    tokyo.graphNetRx = {125, 207, 255};
    tokyo.graphNetTx = {247, 118, 142};
    tokyo.graphDiskRead = {158, 206, 106};
    tokyo.graphDiskWrite = {224, 175, 104};
    themes.push_back(tokyo);

    // 2. Dracula (Rich vampiric dark with pastel neon pops)
    Theme dracula;
    dracula.name = "Dracula";
    dracula.preset = ThemePreset::DRACULA;
    dracula.bg = {40, 42, 54};
    dracula.frameBorder = {98, 114, 164};
    dracula.textMain = {248, 248, 242};
    dracula.textDim = {98, 114, 164};
    dracula.titleActive = {189, 147, 249};
    dracula.cpuBorder = {139, 233, 253};  // Cyan
    dracula.memBorder = {255, 121, 198};  // Pink
    dracula.diskBorder = {241, 250, 140}; // Yellow
    dracula.netBorder = {80, 250, 123};   // Green
    dracula.procBorder = {189, 147, 249}; // Purple
    dracula.meterLow = {80, 250, 123};
    dracula.meterMid = {255, 184, 108};
    dracula.meterHigh = {255, 85, 85};
    dracula.selBg = {68, 71, 90};
    dracula.selFg = {255, 255, 255};
    dracula.graphCpu = {139, 233, 253};
    dracula.graphMem = {255, 121, 198};
    dracula.graphNetRx = {80, 250, 123};
    dracula.graphNetTx = {255, 85, 85};
    dracula.graphDiskRead = {241, 250, 140};
    dracula.graphDiskWrite = {255, 184, 108};
    themes.push_back(dracula);

    // 3. Nord (Arctic, frosty cyan & clean scandinavian calm)
    Theme nord;
    nord.name = "Nord";
    nord.preset = ThemePreset::NORD;
    nord.bg = {46, 52, 64};
    nord.frameBorder = {76, 86, 106};
    nord.textMain = {236, 239, 244};
    nord.textDim = {94, 105, 128};
    nord.titleActive = {136, 192, 208};
    nord.cpuBorder = {136, 192, 208};  // Frost Cyan
    nord.memBorder = {180, 142, 173};  // Aurora Purple
    nord.diskBorder = {235, 203, 139}; // Aurora Yellow
    nord.netBorder = {163, 190, 140};  // Aurora Green
    nord.procBorder = {129, 161, 193}; // Frost Blue
    nord.meterLow = {163, 190, 140};
    nord.meterMid = {235, 203, 139};
    nord.meterHigh = {191, 97, 106};
    nord.selBg = {67, 76, 94};
    nord.selFg = {255, 255, 255};
    nord.graphCpu = {136, 192, 208};
    nord.graphMem = {180, 142, 173};
    nord.graphNetRx = {143, 188, 187};
    nord.graphNetTx = {191, 97, 106};
    nord.graphDiskRead = {163, 190, 140};
    nord.graphDiskWrite = {208, 135, 112};
    themes.push_back(nord);

    // 4. Cyberpunk / Neon (Obsidian darkness with neon lasers)
    Theme cyber;
    cyber.name = "Cyberpunk Neon";
    cyber.preset = ThemePreset::CYBERPUNK;
    cyber.bg = {15, 15, 20};
    cyber.frameBorder = {70, 70, 90};
    cyber.textMain = {240, 240, 255};
    cyber.textDim = {100, 100, 130};
    cyber.titleActive = {0, 255, 220};
    cyber.cpuBorder = {0, 230, 255};    // Electric Cyan
    cyber.memBorder = {255, 0, 128};    // Hot Magenta
    cyber.diskBorder = {255, 220, 0};   // Laser Yellow
    cyber.netBorder = {0, 255, 140};    // Acid Green
    cyber.procBorder = {180, 0, 255};   // Neon Violet
    cyber.meterLow = {0, 255, 180};
    cyber.meterMid = {255, 220, 0};
    cyber.meterHigh = {255, 30, 90};
    cyber.selBg = {180, 0, 120};
    cyber.selFg = {255, 255, 255};
    cyber.graphCpu = {0, 230, 255};
    cyber.graphMem = {255, 0, 128};
    cyber.graphNetRx = {0, 255, 140};
    cyber.graphNetTx = {255, 0, 90};
    cyber.graphDiskRead = {255, 220, 0};
    cyber.graphDiskWrite = {255, 100, 0};
    themes.push_back(cyber);

    // 5. Monokai Pro (Warm contrast with lime & magenta)
    Theme monokai;
    monokai.name = "Monokai Pro";
    monokai.preset = ThemePreset::MONOKAI;
    monokai.bg = {34, 31, 34};
    monokai.frameBorder = {88, 85, 88};
    monokai.textMain = {250, 250, 245};
    monokai.textDim = {120, 115, 120};
    monokai.titleActive = {255, 216, 102};
    monokai.cpuBorder = {120, 220, 232};
    monokai.memBorder = {255, 97, 136};
    monokai.diskBorder = {255, 216, 102};
    monokai.netBorder = {169, 220, 103};
    monokai.procBorder = {171, 157, 242};
    monokai.meterLow = {169, 220, 103};
    monokai.meterMid = {255, 216, 102};
    monokai.meterHigh = {255, 97, 136};
    monokai.selBg = {80, 75, 80};
    monokai.selFg = {255, 255, 255};
    monokai.graphCpu = {120, 220, 232};
    monokai.graphMem = {255, 97, 136};
    monokai.graphNetRx = {169, 220, 103};
    monokai.graphNetTx = {255, 97, 136};
    monokai.graphDiskRead = {255, 216, 102};
    monokai.graphDiskWrite = {252, 152, 103};
    themes.push_back(monokai);

    // 6. Matrix Green (Hacker terminal monochrome aesthetic)
    Theme matrix;
    matrix.name = "Matrix Green";
    matrix.preset = ThemePreset::MATRIX;
    matrix.bg = {10, 16, 10};
    matrix.frameBorder = {30, 80, 30};
    matrix.textMain = {100, 255, 100};
    matrix.textDim = {40, 120, 40};
    matrix.titleActive = {150, 255, 150};
    matrix.cpuBorder = {50, 255, 50};
    matrix.memBorder = {80, 220, 80};
    matrix.diskBorder = {180, 255, 80};
    matrix.netBorder = {0, 255, 120};
    matrix.procBorder = {120, 255, 160};
    matrix.meterLow = {40, 180, 40};
    matrix.meterMid = {140, 240, 60};
    matrix.meterHigh = {255, 80, 80};
    matrix.selBg = {20, 80, 20};
    matrix.selFg = {255, 255, 255};
    matrix.graphCpu = {50, 255, 50};
    matrix.graphMem = {80, 220, 80};
    matrix.graphNetRx = {0, 255, 120};
    matrix.graphNetTx = {255, 120, 50};
    matrix.graphDiskRead = {180, 255, 80};
    matrix.graphDiskWrite = {220, 200, 40};
    themes.push_back(matrix);
}

void ThemeManager::setPreset(ThemePreset preset) {
    size_t idx = static_cast<size_t>(preset);
    if (idx < themes.size()) {
        currentIdx = idx;
    }
}

void ThemeManager::nextTheme() {
    currentIdx = (currentIdx + 1) % themes.size();
}
