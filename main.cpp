#include<iostream>
#include<thread>
#include<chrono>
#include "cpu.h"
#include "memory.h"
#include "process.h"
#include <algorithm>

using namespace std;

long getTotalCPUTime(const CPUStats& stats){
    return stats.user+stats.nice+stats.system+stats.idle
           +stats.iowait+stats.irq+stats.softirq+stats.steal;
}

int main(){

    CPUStats prev = readCPU();
    while(true){
        this_thread::sleep_for(chrono::seconds(2));

        CPUStats curr = readCPU();
        double cpuUsage = calculateCPUUsage(prev, curr);
        double memUsage = getMemoryUsage();

        system("clear");

        cout<<"======System Monitor======\n";
        cout<<"CPU Usage : "<<cpuUsage<<" %\n";
        cout<<"Memory Usage : "<<memUsage<<" %\n";
        cout<<"---------------------------\n";

        auto processes = getProcesses();
        long totalPrev = getTotalCPUTime(prev);
        long totalCurr = getTotalCPUTime(curr);
        long totalDelta = totalCurr - totalPrev;

        std::cout << "TOTAL DELTA: " << totalDelta << std::endl;
        int numCores = thread::hardware_concurrency();

        for (auto& p : processes) {
            if (totalDelta > 0) {
                p.cpu_usage = ((double)p.cpu_usage / totalDelta) * numCores * 100.0;
            } else {
                p.cpu_usage = 0.0;
            }
        }

        sort(processes.begin(), processes.end(),
        [](const Process& a,const Process& b){
            return a.cpu_usage>b.cpu_usage;
        });

        cout<<"\nPID\tCPU%\tNAME\n";
        int count = 0;
        for (const auto& p : processes){
            cout<<p.pid<<"\t"<<p.cpu_usage<<" %\t"<<p.name<<"\n";
            if(++count>=10) break;
        }
        prev = curr;
    }
    return 0;
}