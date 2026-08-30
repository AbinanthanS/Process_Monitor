#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>
#include <cstdint>

struct Process {
    int pid = 0;
    int ppid = 0;
    int threads = 1;
    std::string user = "root";
    int priority = 20;
    int nice = 0;
    uint64_t virt_bytes = 0;
    uint64_t res_bytes = 0;
    uint64_t shr_bytes = 0;
    char state = 'S';
    double cpu_usage = 0.0;
    double mem_usage = 0.0;
    uint64_t cpu_time_ticks = 0;
    uint64_t cpu_time_seconds = 0;
    std::string name;
    std::string cmdline;

    long prev_time = 0;
    long curr_time = 0;
};

struct TaskCounts {
    int total = 0;
    int running = 0;
    int sleeping = 0;
    int stopped = 0;
    int zombie = 0;
};

struct ProcessSnapshot {
    std::vector<Process> processes;
    TaskCounts taskCounts;
};

ProcessSnapshot getProcessesSnapshot(long totalDelta, int numCores, uint64_t totalMemBytes);

#endif // PROCESS_H
