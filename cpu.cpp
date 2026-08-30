#include "cpu.h"
#include <fstream>
#include <sstream>
#include <string>

using namespace std;

long getTotalCPUTime(const CPUStats& stats) {
    return stats.user + stats.nice + stats.system + stats.idle
           + stats.iowait + stats.irq + stats.softirq + stats.steal;
}

double calculateCPUUsage(const CPUStats& prev, const CPUStats& curr) {
    long prevIdle = prev.idle + prev.iowait;
    long currIdle = curr.idle + curr.iowait;

    long prevTotal = getTotalCPUTime(prev);
    long currTotal = getTotalCPUTime(curr);

    long totalDiff = currTotal - prevTotal;
    long idleDiff = currIdle - prevIdle;

    if (totalDiff <= 0) return 0.0;

    double usage = 100.0 * (totalDiff - idleDiff) / static_cast<double>(totalDiff);
    if (usage < 0.0) usage = 0.0;
    if (usage > 100.0) usage = 100.0;
    return usage;
}

SystemCPUInfo readSystemCPU() {
    SystemCPUInfo info{};

    // 1. Read /proc/stat for total and per-core CPU times
    ifstream statFile("/proc/stat");
    if (statFile.is_open()) {
        string line;
        while (getline(statFile, line)) {
            if (line.rfind("cpu", 0) != 0) {
                // Done reading CPU lines
                break;
            }

            istringstream ss(line);
            CPUStats stats;
            ss >> stats.name
               >> stats.user
               >> stats.nice
               >> stats.system
               >> stats.idle
               >> stats.iowait
               >> stats.irq
               >> stats.softirq
               >> stats.steal;

            if (stats.name == "cpu") {
                info.total = stats;
            } else {
                info.cores.push_back(stats);
            }
        }
    }

    // 2. Read /proc/loadavg
    ifstream loadFile("/proc/loadavg");
    if (loadFile.is_open()) {
        loadFile >> info.load1 >> info.load5 >> info.load15;
    }

    // 3. Read /proc/uptime
    ifstream uptimeFile("/proc/uptime");
    if (uptimeFile.is_open()) {
        double uptimeSec = 0.0;
        uptimeFile >> uptimeSec;
        info.uptimeSeconds = static_cast<long>(uptimeSec);
    }

    return info;
}