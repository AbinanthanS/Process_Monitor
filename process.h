#ifndef PRROCESS_H
#define PROCESS_H

#include<string>
#include<vector>

using namespace std;

struct Process{
    int pid;
    string name;
    long prev_time = 0;
    long curr_time = 0;
    double cpu_usage = 0.0;
};

vector<Process> getProcesses();

#endif