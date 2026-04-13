#include "memory.h"
#include <fstream>
#include <string>

using namespace std;

double getMemoryUsage(){

    ifstream file("/proc/meminfo");
    string key;
    long value;
    string unit;

    long total = 0;
    long available = 0;

    while(file>>key>>value>>unit){
        if (key == "MemTotal:"){
            total = value;
        }else if (key == "MemAvailable:"){
            available = value;
            break;
        }
    }

    if (total == 0) return 0.0;

    return 100.0 * (total - available) / total;

}