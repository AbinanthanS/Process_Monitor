#include "collectors/process.h"
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <cctype>

#if defined(_WIN32)
typedef unsigned int uid_t;
#else
#include <pwd.h>
#include <unistd.h>
#endif

using namespace std;

static unordered_map<int, long> prevProcessTimes;
static unordered_map<uid_t, string> uidCache;

static long hertz = 0;
static long pageSize = 0;

static void initSystemConstants() {
#if defined(_WIN32)
    if (hertz == 0) hertz = 100;
    if (pageSize == 0) pageSize = 4096;
#else
    if (hertz == 0) {
        hertz = sysconf(_SC_CLK_TCK);
        if (hertz <= 0) hertz = 100;
    }
    if (pageSize == 0) {
        pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize <= 0) pageSize = 4096;
    }
#endif
}

static string getUsername(uid_t uid) {
#if defined(_WIN32)
    return to_string(uid);
#else
    auto it = uidCache.find(uid);
    if (it != uidCache.end()) {
        return it->second;
    }

    struct passwd* pw = getpwuid(uid);
    string name = (pw != nullptr) ? pw->pw_name : to_string(uid);
    uidCache[uid] = name;
    return name;
#endif
}

static uid_t getProcessUid(int pid) {
    string path = "/proc/" + to_string(pid) + "/status";
    ifstream file(path);
    if (!file.is_open()) return 0;

    string line;
    while (getline(file, line)) {
        if (line.rfind("Uid:", 0) == 0) {
            istringstream ss(line.substr(4));
            uid_t realUid = 0;
            ss >> realUid;
            return realUid;
        }
    }
    return 0;
}

static string getProcessCmdline(int pid, const string& fallbackName) {
    string path = "/proc/" + to_string(pid) + "/cmdline";
    ifstream file(path);
    if (!file.is_open()) return fallbackName;

    string cmdline;
    char ch;
    while (file.get(ch)) {
        if (ch == '\0') cmdline.push_back(' ');
        else cmdline.push_back(ch);
    }

    while (!cmdline.empty() && isspace(cmdline.back())) {
        cmdline.pop_back();
    }

    if (cmdline.empty()) {
        return "[" + fallbackName + "]";
    }
    return cmdline;
}

static bool readProcessStat(int pid, Process& proc) {
    string path = "/proc/" + to_string(pid) + "/stat";
    ifstream file(path);
    if (!file.is_open()) return false;

    string line;
    if (!getline(file, line)) return false;

    auto openParen = line.find('(');
    auto closeParen = line.rfind(')');
    if (openParen == string::npos || closeParen == string::npos || closeParen < openParen) {
        return false;
    }

    proc.pid = pid;
    proc.name = line.substr(openParen + 1, closeParen - openParen - 1);

    string rest = line.substr(closeParen + 2);
    istringstream ss(rest);

    char state;
    int ppid, pgrp, session, tty_nr, tpgid;
    unsigned int flags;
    unsigned long minflt, cminflt, majflt, cmajflt, utime, stime;
    long cutime, cstime, priority, nice, num_threads, itrealvalue;
    unsigned long long starttime;
    unsigned long vsize;
    long rss;

    if (!(ss >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid
             >> flags >> minflt >> cminflt >> majflt >> cmajflt
             >> utime >> stime >> cutime >> cstime
             >> priority >> nice >> num_threads >> itrealvalue
             >> starttime >> vsize >> rss)) {
        return false;
    }

    proc.state = state;
    proc.ppid = ppid;
    proc.priority = priority;
    proc.nice = nice;
    proc.threads = num_threads;
    proc.cpu_time_ticks = utime + stime;
    proc.virt_bytes = vsize;
    proc.res_bytes = rss * pageSize;

    return true;
}

static void readProcessStatm(int pid, Process& proc) {
    string path = "/proc/" + to_string(pid) + "/statm";
    ifstream file(path);
    if (!file.is_open()) return;

    unsigned long size, resident, shared, text, lib, data, dt;
    if (file >> size >> resident >> shared >> text >> lib >> data >> dt) {
        proc.shr_bytes = shared * pageSize;
    }
}

ProcessSnapshot getProcessesSnapshot(long totalCpuDeltaTicks, int numCores, uint64_t totalMemBytes) {
    initSystemConstants();

    ProcessSnapshot snapshot;
    snapshot.taskCounts = {0, 0, 0, 0, 0};

    DIR* dir = opendir("/proc");
    if (!dir) return snapshot;

    unordered_map<int, long> currProcessTimes;
    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        char* endptr = nullptr;
        long pid = strtol(entry->d_name, &endptr, 10);
        if (*endptr != '\0' || pid <= 0) continue;

        Process proc;
        if (!readProcessStat(pid, proc)) continue;

        readProcessStatm(pid, proc);

        proc.user = getUsername(getProcessUid(pid));
        proc.cmdline = getProcessCmdline(pid, proc.name);

        snapshot.taskCounts.total++;
        switch (proc.state) {
            case 'R': snapshot.taskCounts.running++; break;
            case 'S':
            case 'D': snapshot.taskCounts.sleeping++; break;
            case 'T':
            case 't': snapshot.taskCounts.stopped++; break;
            case 'Z': snapshot.taskCounts.zombie++; break;
            default: break;
        }

        currProcessTimes[pid] = proc.cpu_time_ticks;

        if (totalCpuDeltaTicks > 0) {
            auto it = prevProcessTimes.find(pid);
            if (it != prevProcessTimes.end()) {
                long procDelta = proc.cpu_time_ticks - it->second;
                if (procDelta < 0) procDelta = 0;
                proc.cpu_usage = (static_cast<double>(procDelta) / totalCpuDeltaTicks) * 100.0 * numCores;
            } else {
                proc.cpu_usage = 0.0;
            }
        }

        if (totalMemBytes > 0) {
            proc.mem_usage = (static_cast<double>(proc.res_bytes) / totalMemBytes) * 100.0;
        }

        proc.cpu_time_seconds = proc.cpu_time_ticks / (hertz > 0 ? hertz : 100);

        snapshot.processes.push_back(std::move(proc));
    }

    closedir(dir);

    prevProcessTimes = std::move(currProcessTimes);

    return snapshot;
}
