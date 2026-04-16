#ifndef PRROCESS_H
#define PROCESS_H

#include<string>
#include<vector>

using namespace std;

struct Process{
    int pid;
    string name;
};

vector<Process> getProcesses();

#endif