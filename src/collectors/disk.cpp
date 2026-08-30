#include "collectors/disk.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#if defined(__unix__) || defined(__linux__)
#include <sys/statvfs.h>
#endif

using namespace std;

struct RawDiskSample {
    uint64_t readsCompleted = 0;
    uint64_t sectorsRead = 0;
    uint64_t writesCompleted = 0;
    uint64_t sectorsWritten = 0;
};

static unordered_map<string, RawDiskSample> prevDiskSamples;

static bool isPhysicalDisk(const string& name) {
    if (name.rfind("loop", 0) == 0 || name.rfind("ram", 0) == 0 || name.rfind("zram", 0) == 0) {
        return false;
    }
    return true;
}

static vector<MountInfo> readMountedFileSystems() {
    vector<MountInfo> mounts;
    unordered_set<string> seenMounts;

    ifstream file("/proc/mounts");
    if (!file.is_open()) {
        file.open("/etc/mtab");
    }

    if (!file.is_open()) {
        return mounts;
    }

    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        istringstream ss(line);
        string dev, mountPoint, fsType, options;
        if (!(ss >> dev >> mountPoint >> fsType >> options)) continue;

        if (fsType == "proc" || fsType == "sysfs" || fsType == "cgroup" ||
            fsType == "cgroup2" || fsType == "pstore" || fsType == "bpf" ||
            fsType == "securityfs" || fsType == "configfs" || fsType == "debugfs" ||
            fsType == "tracefs" || fsType == "fusectl" || fsType == "hugetlbfs" ||
            fsType == "mqueue" || fsType == "devpts" || fsType == "autofs" ||
            fsType == "binfmt_misc" || fsType == "rpc_pipefs" || fsType == "nsfs") {
            continue;
        }

        if (seenMounts.count(mountPoint)) continue;

        if (dev.rfind("/dev/", 0) != 0 && fsType != "zfs" && fsType != "btrfs" && fsType != "nfs" && fsType != "cifs" && fsType != "fuse.sshfs") {
            if (mountPoint != "/" && mountPoint != "/home") {
                continue;
            }
        }

        if (mountPoint.rfind("/var/lib/docker", 0) == 0 || mountPoint.rfind("/snap", 0) == 0) {
            continue;
        }

#if defined(__unix__) || defined(__linux__)
        struct statvfs vfs{};
        if (statvfs(mountPoint.c_str(), &vfs) == 0 && vfs.f_blocks > 0) {
            uint64_t frsize = vfs.f_frsize > 0 ? vfs.f_frsize : vfs.f_bsize;
            uint64_t total = vfs.f_blocks * frsize;
            uint64_t free = vfs.f_bavail * frsize;
            uint64_t used = (vfs.f_blocks >= vfs.f_bfree) ? (vfs.f_blocks - vfs.f_bfree) * frsize : (total - free);
            double pct = total > 0 ? (static_cast<double>(used) / total) * 100.0 : 0.0;

            MountInfo mi;
            mi.mountPoint = mountPoint;
            mi.device = dev;
            mi.fsType = fsType;
            mi.totalBytes = total;
            mi.usedBytes = used;
            mi.freeBytes = free;
            mi.usedPercent = pct;

            mounts.push_back(mi);
            seenMounts.insert(mountPoint);
        }
#endif
    }

    sort(mounts.begin(), mounts.end(), [](const MountInfo& a, const MountInfo& b) {
        if (a.mountPoint == "/") return true;
        if (b.mountPoint == "/") return false;
        return a.mountPoint < b.mountPoint;
    });

    return mounts;
}

SystemDiskInfo readDiskStats(double deltaSeconds) {
    SystemDiskInfo info{};
    if (deltaSeconds <= 0.0) deltaSeconds = 1.0;

    info.mounts = readMountedFileSystems();

    ifstream file("/proc/diskstats");
    if (!file.is_open()) return info;

    string line;
    while (getline(file, line)) {
        istringstream ss(line);
        int major = 0, minor = 0;
        string devName;
        uint64_t reads = 0, readsMerged = 0, sectorsRead = 0, readMs = 0;
        uint64_t writes = 0, writesMerged = 0, sectorsWritten = 0, writeMs = 0;

        if (ss >> major >> minor >> devName >> reads >> readsMerged >> sectorsRead >> readMs
               >> writes >> writesMerged >> sectorsWritten >> writeMs) {
            
            if (!isPhysicalDisk(devName)) continue;
            if (sectorsRead == 0 && sectorsWritten == 0) continue;

            uint64_t totalReadB = sectorsRead * 512ULL;
            uint64_t totalWriteB = sectorsWritten * 512ULL;

            RawDiskSample currSample{reads, sectorsRead, writes, sectorsWritten};

            DiskStats ds;
            ds.name = devName;
            ds.totalReadBytes = totalReadB;
            ds.totalWriteBytes = totalWriteB;

            auto it = prevDiskSamples.find(devName);
            if (it != prevDiskSamples.end()) {
                uint64_t deltaReadSectors = (sectorsRead >= it->second.sectorsRead) ? (sectorsRead - it->second.sectorsRead) : 0;
                uint64_t deltaWriteSectors = (sectorsWritten >= it->second.sectorsWritten) ? (sectorsWritten - it->second.sectorsWritten) : 0;
                uint64_t deltaReads = (reads >= it->second.readsCompleted) ? (reads - it->second.readsCompleted) : 0;
                uint64_t deltaWrites = (writes >= it->second.writesCompleted) ? (writes - it->second.writesCompleted) : 0;

                ds.readBytesSec = static_cast<uint64_t>((deltaReadSectors * 512.0) / deltaSeconds);
                ds.writeBytesSec = static_cast<uint64_t>((deltaWriteSectors * 512.0) / deltaSeconds);
                ds.iopsSec = static_cast<uint64_t>((deltaReads + deltaWrites) / deltaSeconds);
            }

            prevDiskSamples[devName] = currSample;

            info.totalReadBytesSec += ds.readBytesSec;
            info.totalWriteBytesSec += ds.writeBytesSec;
            info.totalIopsSec += ds.iopsSec;
            info.disks.push_back(ds);
        }
    }

    return info;
}
