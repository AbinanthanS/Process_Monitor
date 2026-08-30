#include "process.h"
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <pwd.h>
#include <unistd.h>
#include <cctype>

using namespace std;

static unordered_map<int, long> prevProcessTimes;
static unordered_map<uid_t, string> uidCache;

static long hertz = 0;
static long pageSize = 0;

static void initSystemConstants() {
    if (hertz == 0) {
        hertz = sysconf(_SC_CLK_TCK);
        if (hertz <= 0) hertz = 100;
    }
    if (pageSize == 0) {
        pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize <= 0) pageSize = 4096;
    }
}

static string getUsername(uid_t uid) {
    auto it = uidCache.find(uid);
    if (it != uidCache.end()) {
        return it->second;
    }

    struct passwd* pw = getpwuid(uid);
    string name = (pw != nullptr) ? pw->pw_name : to_string(uid);
    uidCache[uid] = name;
    return name;
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

static bool isNumber(const string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!isdigit(c)) return false;
    }
    return true;
}

ProcessSnapshot getProcessesSnapshot(long totalDelta, int numCores, uint64_t totalMemBytes) {
    initSystemConstants();

    ProcessSnapshot snapshot;
    set<int> currentPids;

    DIR* dir = opendir("/proc");
    if (dir == nullptr) {
        return snapshot;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        string dirName = entry->d_name;
        if (!isNumber(dirName)) continue;

        int pid = stoi(dirName);
        currentPids.insert(pid);

        // 1. Read /proc/[pid]/stat
        string statPath = "/proc/" + dirName + "/stat";
        ifstream statFile(statPath);
        if (!statFile.is_open()) continue;

        string statLine;
        if (!getline(statFile, statLine)) continue;

        auto openParen = statLine.find('(');
        auto closeParen = statLine.rfind(')');
        if (openParen == string::npos || closeParen == string::npos || closeParen < openParen) {
            continue;
        }

        string comm = statLine.substr(openParen + 1, closeParen - openParen - 1);
        string restOfStat = statLine.substr(closeParen + 2);
        istringstream statSS(restOfStat);

        char state = 'S';
        int ppid = 0, pgrp = 0, session = 0, tty_nr = 0, tpgid = 0;
        unsigned int flags = 0;
        unsigned long minflt = 0, cminflt = 0, majflt = 0, cmajflt = 0;
        unsigned long utime = 0, stime = 0;
        long cutime = 0, cstime = 0, priority = 20, nice = 0, num_threads = 1;
        long itrealvalue = 0;
        unsigned long long starttime = 0;
        unsigned long vsize = 0;
        long rss = 0;

        statSS >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags
               >> minflt >> cminflt >> majflt >> cmajflt
               >> utime >> stime >> cutime >> cstime
               >> priority >> nice >> num_threads >> itrealvalue >> starttime
               >> vsize >> rss;

        snapshot.taskCounts.total++;
        switch (state) {
            case 'R': snapshot.taskCounts.running++; break;
            case 'S':
            case 'I':
            case 'D': snapshot.taskCounts.sleeping++; break;
            case 'T':
            case 't': snapshot.taskCounts.stopped++; break;
            case 'Z': snapshot.taskCounts.zombie++; break;
            default: snapshot.taskCounts.sleeping++; break;
        }

        // 2. Read /proc/[pid]/statm
        string statmPath = "/proc/" + dirName + "/statm";
        ifstream statmFile(statmPath);
        uint64_t sizePages = 0, residentPages = 0, sharedPages = 0;
        if (statmFile.is_open()) {
            statmFile >> sizePages >> residentPages >> sharedPages;
        } else {
            residentPages = static_cast<uint64_t>(rss > 0 ? rss : 0);
            sizePages = vsize / pageSize;
        }

        // 3. Read /proc/[pid]/cmdline
        string cmdlinePath = "/proc/" + dirName + "/cmdline";
        ifstream cmdlineFile(cmdlinePath);
        string cmdline;
        if (cmdlineFile.is_open()) {
            getline(cmdlineFile, cmdline);
            for (char& ch : cmdline) {
                if (ch == '\0') ch = ' ';
            }
        }
        if (cmdline.empty()) {
            cmdline = "[" + comm + "]";
        }

        // 4. Resolve username
        uid_t uid = getProcessUid(pid);
        string username = getUsername(uid);

        // 5. Calculate CPU usage delta
        long totalProcTicks = utime + stime;
        long prevTime = prevProcessTimes[pid];
        long delta = totalProcTicks - prevTime;

        Process proc;
        proc.pid = pid;
        proc.ppid = ppid;
        proc.user = username;
        proc.priority = static_cast<int>(priority);
        proc.nice = static_cast<int>(nice);
        proc.virt_bytes = sizePages * pageSize;
        proc.res_bytes = residentPages * pageSize;
        proc.shr_bytes = sharedPages * pageSize;
        proc.state = state;
        proc.name = comm;
        proc.cmdline = cmdline;
        proc.cpu_time_ticks = totalProcTicks;
        proc.cpu_time_seconds = totalProcTicks / hertz;
        proc.prev_time = prevTime;
        proc.curr_time = totalProcTicks;

        prevProcessTimes[pid] = totalProcTicks;

        if (prevTime > 0 && totalDelta > 0 && delta > 0) {
            double usage = (static_cast<double>(delta) / totalDelta) * numCores * 100.0;
            proc.cpu_usage = min(usage, static_cast<double>(numCores * 100.0));
        } else {
            proc.cpu_usage = 0.0;
        }

        if (totalMemBytes > 0) {
            proc.mem_usage = (static_cast<double>(proc.res_bytes) / totalMemBytes) * 100.0;
        }

        snapshot.processes.push_back(proc);
    }

    closedir(dir);

    for (auto it = prevProcessTimes.begin(); it != prevProcessTimes.end();) {
        if (currentPids.find(it->first) == currentPids.end()) {
            it = prevProcessTimes.erase(it);
        } else {
            ++it;
        }
    }

    return snapshot;
}
