#include "process.h"
#include <dirent.h>
#include <fstream>
#include <string>
#include <vector>
#include<algorithm>

using namespace std;

bool isNumber(const string& s) {
    for (char c : s) {
        if (!isdigit(c)) return false;
    }
    return !s.empty();
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

                // cmdline uses '\0' as separator → replace with space
                replace(name.begin(), name.end(), '\0', ' ');

                if (name.empty()) {
                    name = "[kernel_process]";
                }

                processes.push_back({pid, name});
            }
        }
    }

    closedir(dir);
    return processes;
}