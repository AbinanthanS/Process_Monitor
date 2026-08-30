#include "memory.h"
#include <fstream>
#include <string>

using namespace std;

MemoryInfo getMemoryInfo() {
    ifstream file("/proc/meminfo");
    string key;
    uint64_t value = 0;
    string unit;

    MemoryInfo info{};
    uint64_t sReclaimable = 0;

    while (file >> key >> value >> unit) {
        // Values in /proc/meminfo are in kB, multiply by 1024 to get bytes
        uint64_t bytes = value * 1024ULL;

        if (key == "MemTotal:") {
            info.totalBytes = bytes;
        } else if (key == "MemFree:") {
            info.freeBytes = bytes;
        } else if (key == "MemAvailable:") {
            info.availableBytes = bytes;
        } else if (key == "Buffers:") {
            info.buffersBytes = bytes;
        } else if (key == "Cached:") {
            info.cachedBytes = bytes;
        } else if (key == "SReclaimable:") {
            sReclaimable = bytes;
        } else if (key == "SwapTotal:") {
            info.swapTotalBytes = bytes;
        } else if (key == "SwapFree:") {
            info.swapFreeBytes = bytes;
        }
    }

    if (info.totalBytes > 0) {
        if (info.availableBytes > 0) {
            info.usedBytes = (info.totalBytes >= info.availableBytes) ? (info.totalBytes - info.availableBytes) : 0;
        } else {
            // Fallback for older kernels: total - free - buffers - cached
            uint64_t freeAndCache = info.freeBytes + info.buffersBytes + info.cachedBytes + sReclaimable;
            info.usedBytes = (info.totalBytes >= freeAndCache) ? (info.totalBytes - freeAndCache) : 0;
        }
        info.memUsagePercent = (static_cast<double>(info.usedBytes) / info.totalBytes) * 100.0;
    }

    if (info.swapTotalBytes > 0) {
        info.swapUsedBytes = (info.swapTotalBytes >= info.swapFreeBytes) ? (info.swapTotalBytes - info.swapFreeBytes) : 0;
        info.swapUsagePercent = (static_cast<double>(info.swapUsedBytes) / info.swapTotalBytes) * 100.0;
    }

    return info;
}

double getMemoryUsage() {
    return getMemoryInfo().memUsagePercent;
}