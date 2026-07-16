#ifndef CPU_H
#define CPU_H
#include <cstdint>

struct CPUStats{
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
};
/*
the above struct is used to store the raw CPU statistics read from /proc/stat. 
Each member corresponds to a specific CPU time category, such as user time, system time, idle time, etc. 
This struct can be used to calculate CPU usage by comparing the values at different time intervals.
*/

CPUStats readCPU();
double calculateCPUUsage(const CPUStats& prev, const CPUStats& curr);

#endif 