#ifndef NET_H
#define NET_H

#include <string>
#include <vector>
#include <cstdint>

struct InterfaceStats {
    std::string name;
    uint64_t rxBytesSec = 0;
    uint64_t txBytesSec = 0;
    uint64_t rxPacketsSec = 0;
    uint64_t txPacketsSec = 0;
    uint64_t totalRxBytes = 0;
    uint64_t totalTxBytes = 0;
};

struct SystemNetInfo {
    std::vector<InterfaceStats> interfaces;
    uint64_t totalRxBytesSec = 0;
    uint64_t totalTxBytesSec = 0;
};

SystemNetInfo readNetStats(double deltaSeconds);

#endif // NET_H
