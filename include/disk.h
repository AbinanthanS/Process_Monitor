#ifndef DISK_H
#define DISK_H

#include <string>
#include <vector>
#include <cstdint>

struct DiskStats {
    std::string name;
    uint64_t readBytesSec = 0;
    uint64_t writeBytesSec = 0;
    uint64_t iopsSec = 0;
    uint64_t totalReadBytes = 0;
    uint64_t totalWriteBytes = 0;
};

struct SystemDiskInfo {
    std::vector<DiskStats> disks;
    uint64_t totalReadBytesSec = 0;
    uint64_t totalWriteBytesSec = 0;
    uint64_t totalIopsSec = 0;
};

SystemDiskInfo readDiskStats(double deltaSeconds);

#endif // DISK_H
