#ifndef PROCESS_H
#define PROCESS_H

#include<string>
#include<vector>

struct Process{
    int pid;
    std::string name;
    long prev_time = 0;
    long curr_time = 0;
    double cpu_usage = 0.0;
};

std::vector<Process> getProcesses();

#endif