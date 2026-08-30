#include "collectors/memory.h"
#include <fstream>
#include <sstream>
#include <string>

using namespace std;

MemoryInfo getMemoryInfo() {
    MemoryInfo info{};
    ifstream file("/proc/meminfo");
    if (!file.is_open()) return info;

    string line;
    while (getline(file, line)) {
        istringstream ss(line);
        string key;
        uint64_t valKb = 0;
        string unit;

        if (ss >> key >> valKb >> unit) {
            uint64_t bytes = valKb * 1024ULL;
            if (key == "MemTotal:") info.totalBytes = bytes;
            else if (key == "MemAvailable:") info.availableBytes = bytes;
            else if (key == "MemFree:") info.freeBytes = bytes;
            else if (key == "Buffers:") info.buffersBytes = bytes;
            else if (key == "Cached:") info.cachedBytes = bytes;
            else if (key == "SwapTotal:") info.swapTotalBytes = bytes;
            else if (key == "SwapFree:") info.swapFreeBytes = bytes;
        }
    }

    if (info.totalBytes > 0) {
        if (info.availableBytes > 0 && info.totalBytes >= info.availableBytes) {
            info.usedBytes = info.totalBytes - info.availableBytes;
        } else {
            uint64_t nonUsed = info.freeBytes + info.buffersBytes + info.cachedBytes;
            info.usedBytes = (info.totalBytes >= nonUsed) ? (info.totalBytes - nonUsed) : 0;
        }
        info.memUsagePercent = (static_cast<double>(info.usedBytes) / info.totalBytes) * 100.0;
    }

    if (info.swapTotalBytes > 0 && info.swapTotalBytes >= info.swapFreeBytes) {
        info.swapUsedBytes = info.swapTotalBytes - info.swapFreeBytes;
        info.swapUsagePercent = (static_cast<double>(info.swapUsedBytes) / info.swapTotalBytes) * 100.0;
    }

    return info;
}

double getMemoryUsage() {
    return getMemoryInfo().memUsagePercent;
}
