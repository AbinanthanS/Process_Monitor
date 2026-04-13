#include<iostream>
#include<thread>
#include<chrono>
#include "cpu.h"
#include "memory.h"

using namespace std;

int main(){

    CPUStats prev = readCPU();
    while(true){
        this_thread::sleep_for(chrono::seconds(1));

        CPUStats curr = readCPU();
        double cpuUsage = calculateCPUUsage(prev, curr);
        double memUsage = getMemoryUsage();

        system("clear"); // Clear the console for better readability

        cout<<"======System Monitor======\n";
        cout<<"CPU Usage : "<<cpuUsage<<" %\n";
        cout<<"Memory Usage : "<<memUsage<<" %\n";
        cout<<"==========================\n";

        prev = curr;
    }

    return 0;
}