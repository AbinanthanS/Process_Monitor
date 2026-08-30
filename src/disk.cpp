#include "disk.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>

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
    // Filter out partitions if whole disk is present (e.g. sda1 vs sda), or keep main active drives
    return true;
}

SystemDiskInfo readDiskStats(double deltaSeconds) {
    SystemDiskInfo info{};
    if (deltaSeconds <= 0.0) deltaSeconds = 1.0;

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
            // Focus on root devices or primary partitions
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
