#ifndef CPU_H
#define CPU_H

#include <vector>
#include <string>

struct CPUStats {
    std::string name; // "cpu", "cpu0", "cpu1", etc.
    long user = 0;
    long nice = 0;
    long system = 0;
    long idle = 0;
    long iowait = 0;
    long irq = 0;
    long softirq = 0;
    long steal = 0;
};

struct SystemCPUInfo {
    CPUStats total;
    std::vector<CPUStats> cores;
    double load1 = 0.0;
    double load5 = 0.0;
    double load15 = 0.0;
    long uptimeSeconds = 0;
};

SystemCPUInfo readSystemCPU();
double calculateCPUUsage(const CPUStats& prev, const CPUStats& curr);
long getTotalCPUTime(const CPUStats& stats);

#endif // CPU_H