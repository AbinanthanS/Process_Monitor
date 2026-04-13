#include "cpu.h"
#include <fstream>
#include <string>

using namespace std;

CPUStats readCPU(){

    ifstream file("/proc/stat");
    string cpu;

    CPUStats stats{};

    if (file.is_open()){
        file >> cpu >> stats.user >> stats.nice >> stats.system >> stats.idle 
             >> stats.iowait >> stats.irq >> stats.softirq >> stats.steal;
    }

    return stats;
}

double calculateCPUUsage(const CPUStats& prev, const CPUStats& curr){
    long prevIdle = prev.idle + prev.iowait;
    long currIdle = curr.idle + curr.iowait;

    long prevTotal = prev.user + prev.nice + prev.system + prev.idle + prev.iowait + prev.irq + prev.softirq + prev.steal;
    long currTotal = curr.user + curr.nice + curr.system + curr.idle + curr.iowait + curr.irq + curr.softirq + curr.steal;

    long totalDiff = currTotal - prevTotal;
    long idleDiff = currIdle - prevIdle;

    if (totalDiff == 0) return 0.0; // To Avoid division by zero

    return 100.0 * (totalDiff - idleDiff) / totalDiff;
}