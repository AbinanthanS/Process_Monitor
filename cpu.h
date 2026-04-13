#ifndef CPU_H
#define CPU_H

struct CPUStats{
    long user, nice, system, idle, iowait, irq, softirq, steal;
};
/*
the above struct is used to store the raw CPU statistics read from /proc/stat. 
Each member corresponds to a specific CPU time category, such as user time, system time, idle time, etc. 
This struct can be used to calculate CPU usage by comparing the values at different time intervals.
*/

CPUStats readCPU();
double calculateCPUUsage(const CPUStats& prev, const CPUStats& curr);

#endif // CPU_H