#include "process.h"
#include <dirent.h>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <map>

using namespace std;

static map<int, long> prevProcessTimes;

bool isNumber(const string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!isdigit(c)) return false;
    }
    return true;
}

long getProcessCPUTime(int pid) {
    string path = "/proc/" + to_string(pid) + "/stat";
    ifstream file(path);

    if (!file.is_open()) return 0;

    string line;
    getline(file, line);

    istringstream ss(line);

    string token;
    vector<std::string> fields;

    while (ss >> token) {
        fields.push_back(token);
    }

    if (fields.size() < 17) return 0;

    long utime = stol(fields[13]);
    long stime = stol(fields[14]);

    return utime + stime;
}


vector<Process> getProcesses() {
    vector<Process> processes;

    DIR* dir = opendir("/proc");
    if (dir == nullptr) {
        perror("Failed to open /proc");
        return processes;
    }

    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        string dirName = entry->d_name;

        // Only numeric directories (PIDs)
        if (isNumber(dirName)) {
            int pid = stoi(dirName);

            string path = "/proc/" + dirName + "/cmdline";
            ifstream file(path);

            string name;
            

            if (file.is_open()) {
                getline(file, name);

                // cmdline uses '\0' as separator and it is replaced with space
                replace(name.begin(), name.end(), '\0', ' ');

                if (name.empty()) {
                    name = "[kernel_process]";
                }

                 long currTime = getProcessCPUTime(pid);
                 long prevTime = prevProcessTimes[pid];

                 long delta = currTime - prevTime;

                 Process proc;
                 proc.pid = pid;
                 proc.name = name;
                 proc.prev_time = prevTime;
                 proc.curr_time = currTime;

                 // store for next iteration
                 prevProcessTimes[pid] = currTime;

                 // store delta (will normalize in main)
                 proc.cpu_usage = delta;

                 processes.push_back(proc);
            }
        }
    }

    closedir(dir);
    return processes;
}

