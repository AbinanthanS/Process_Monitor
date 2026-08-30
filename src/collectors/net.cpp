#include "collectors/net.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>

using namespace std;

struct RawNetSample {
    uint64_t rxBytes = 0;
    uint64_t rxPackets = 0;
    uint64_t txBytes = 0;
    uint64_t txPackets = 0;
};

static unordered_map<string, RawNetSample> prevNetSamples;

SystemNetInfo readNetStats(double deltaSeconds) {
    SystemNetInfo info{};
    if (deltaSeconds <= 0.0) deltaSeconds = 1.0;

    ifstream file("/proc/net/dev");
    if (!file.is_open()) return info;

    string line;
    getline(file, line);
    getline(file, line);

    while (getline(file, line)) {
        auto colonPos = line.find(':');
        if (colonPos == string::npos) continue;

        string ifaceName = line.substr(0, colonPos);
        while (!ifaceName.empty() && isspace(ifaceName.front())) ifaceName.erase(0, 1);
        while (!ifaceName.empty() && isspace(ifaceName.back())) ifaceName.pop_back();

        string rest = line.substr(colonPos + 1);
        istringstream ss(rest);

        uint64_t rxBytes = 0, rxPackets = 0, rxErrs = 0, rxDrop = 0, rxFifo = 0, rxFrame = 0, rxComp = 0, rxMcast = 0;
        uint64_t txBytes = 0, txPackets = 0, txErrs = 0, txDrop = 0, txFifo = 0, txColls = 0, txCarrier = 0, txComp = 0;

        if (ss >> rxBytes >> rxPackets >> rxErrs >> rxDrop >> rxFifo >> rxFrame >> rxComp >> rxMcast
               >> txBytes >> txPackets >> txErrs >> txDrop >> txFifo >> txColls >> txCarrier >> txComp) {

            InterfaceStats istat;
            istat.name = ifaceName;
            istat.totalRxBytes = rxBytes;
            istat.totalTxBytes = txBytes;

            RawNetSample currSample{rxBytes, rxPackets, txBytes, txPackets};
            auto it = prevNetSamples.find(ifaceName);
            if (it != prevNetSamples.end()) {
                uint64_t deltaRx = (rxBytes >= it->second.rxBytes) ? (rxBytes - it->second.rxBytes) : 0;
                uint64_t deltaTx = (txBytes >= it->second.txBytes) ? (txBytes - it->second.txBytes) : 0;
                uint64_t deltaRxPkts = (rxPackets >= it->second.rxPackets) ? (rxPackets - it->second.rxPackets) : 0;
                uint64_t deltaTxPkts = (txPackets >= it->second.txPackets) ? (txPackets - it->second.txPackets) : 0;

                istat.rxBytesSec = static_cast<uint64_t>(deltaRx / deltaSeconds);
                istat.txBytesSec = static_cast<uint64_t>(deltaTx / deltaSeconds);
                istat.rxPacketsSec = static_cast<uint64_t>(deltaRxPkts / deltaSeconds);
                istat.txPacketsSec = static_cast<uint64_t>(deltaTxPkts / deltaSeconds);
            }

            prevNetSamples[ifaceName] = currSample;

            if (ifaceName != "lo") {
                info.totalRxBytesSec += istat.rxBytesSec;
                info.totalTxBytesSec += istat.txBytesSec;
            }
            info.interfaces.push_back(istat);
        }
    }

    sort(info.interfaces.begin(), info.interfaces.end(), [](const InterfaceStats& a, const InterfaceStats& b) {
        if (a.name == "lo") return false;
        if (b.name == "lo") return true;
        return a.name < b.name;
    });

    return info;
}
