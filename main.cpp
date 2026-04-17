#include<iostream>
#include<thread>
#include<chrono>
#include "cpu.h"
#include "memory.h"
#include "process.h"

using namespace std;

int main(){

    CPUStats prev = readCPU();
    while(true){
        this_thread::sleep_for(chrono::seconds(1));

        CPUStats curr = readCPU();
        double cpuUsage = calculateCPUUsage(prev, curr);
        double memUsage = getMemoryUsage();

        system("clear");

        cout<<"======System Monitor======\n";
        cout<<"CPU Usage : "<<cpuUsage<<" %\n";
        cout<<"Memory Usage : "<<memUsage<<" %\n";
        cout<<"---------------------------\n";

        auto processes = getProcesses();
        cout<<"\nPID\tNAME\n";
        int count = 0;
        for (const auto& p:processes){
            cout<<p.pid<<"\t"<<p.name<<"\n";
            if (++count>=10) break;
        }
        prev = curr;
    }

    return 0;
}