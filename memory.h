#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>

struct MemoryInfo {
    uint64_t totalBytes = 0;
    uint64_t availableBytes = 0;
    uint64_t freeBytes = 0;
    uint64_t usedBytes = 0;
    uint64_t buffersBytes = 0;
    uint64_t cachedBytes = 0;

    uint64_t swapTotalBytes = 0;
    uint64_t swapFreeBytes = 0;
    uint64_t swapUsedBytes = 0;

    double memUsagePercent = 0.0;
    double swapUsagePercent = 0.0;
};

MemoryInfo getMemoryInfo();
double getMemoryUsage();

#endif // MEMORY_H