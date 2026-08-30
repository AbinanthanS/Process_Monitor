#include "core/app.h"
#include <iostream>
#include <string>
#include <vector>

void printHelp(const char* progName) {
    std::cout << "btop++ System & Process Monitor\n\n"
              << "Usage: " << progName << " [options]\n\n"
              << "Module Selection Options:\n"
              << "  --cpu, -c         Enable CPU module\n"
              << "  --mem, -m         Enable Memory/Swap module\n"
              << "  --disk, -d        Enable Disk & Filesystem module\n"
              << "  --net, -n         Enable Network module\n"
              << "  --proc, -p        Enable Process table module\n"
              << "  --no-cpu          Disable CPU module\n"
              << "  --no-mem          Disable Memory module\n"
              << "  --no-disk         Disable Disk module\n"
              << "  --no-net          Disable Network module\n"
              << "  --no-proc         Disable Process module\n"
              << "  --preset <name>   Preset: full, resources, proc, io, minimal\n"
              << "  -i, --interval <ms> Set refresh interval in milliseconds (default: 1000)\n"
              << "  -h, --help        Show this help message\n\n"
              << "Interactive Hotkeys inside Monitor:\n"
              << "  1, 2, 3, 4, 5     Toggle CPU, MEM, DISK, NET, PROC panels independently\n"
              << "  Tab / P           Cycle layout presets\n"
              << "  m / F2            Open Module configuration menu\n"
              << "  i / I             Cycle network interface (eth0, wlan0, etc.)\n"
              << "  / or F3           Search & filter processes\n"
              << "  c, e, p, t, u, n  Sort by CPU, MEM, PID, TIME, USER, NAME\n"
              << "  k or F9           Send signal / kill process\n"
              << "  + / -             Adjust refresh rate\n"
              << "  Space             Pause / Resume\n"
              << "  q / F10           Quit\n";
}

int main(int argc, char* argv[]) {
    try {
        App app;

        bool explicitOnlyMode = false;
        std::vector<std::string> enableList;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help") {
                printHelp(argv[0]);
                return 0;
            } else if (arg == "-c" || arg == "--cpu") {
                explicitOnlyMode = true;
                enableList.push_back("cpu");
            } else if (arg == "-m" || arg == "--mem") {
                explicitOnlyMode = true;
                enableList.push_back("mem");
            } else if (arg == "-d" || arg == "--disk") {
                explicitOnlyMode = true;
                enableList.push_back("disk");
            } else if (arg == "-n" || arg == "--net") {
                explicitOnlyMode = true;
                enableList.push_back("net");
            } else if (arg == "-p" || arg == "--proc") {
                explicitOnlyMode = true;
                enableList.push_back("proc");
            } else if (arg == "--no-cpu") {
                app.setModuleEnabled("cpu", false);
            } else if (arg == "--no-mem") {
                app.setModuleEnabled("mem", false);
            } else if (arg == "--no-disk") {
                app.setModuleEnabled("disk", false);
            } else if (arg == "--no-net") {
                app.setModuleEnabled("net", false);
            } else if (arg == "--no-proc") {
                app.setModuleEnabled("proc", false);
            } else if (arg == "--preset" && i + 1 < argc) {
                std::string presetStr = argv[++i];
                if (presetStr == "full") app.applyPreset(LayoutPreset::FULL);
                else if (presetStr == "resources") app.applyPreset(LayoutPreset::RESOURCES);
                else if (presetStr == "proc") app.applyPreset(LayoutPreset::PROCESSES);
                else if (presetStr == "io") app.applyPreset(LayoutPreset::IO_FOCUS);
                else if (presetStr == "minimal") app.applyPreset(LayoutPreset::MINIMAL);
            } else if ((arg == "-i" || arg == "--interval") && i + 1 < argc) {
                int interval = std::stoi(argv[++i]);
                if (interval >= 100 && interval <= 10000) {
                    app.setRefreshInterval(interval);
                }
            }
        }

        if (explicitOnlyMode) {
            app.setModuleEnabled("cpu", false);
            app.setModuleEnabled("mem", false);
            app.setModuleEnabled("disk", false);
            app.setModuleEnabled("net", false);
            app.setModuleEnabled("proc", false);
            for (const auto& mod : enableList) {
                app.setModuleEnabled(mod, true);
            }
        }

        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
