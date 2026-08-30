#include "collectors/sensors.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

SensorInfo readSensors() {
    SensorInfo info{};

    const vector<string> thermalPaths = {
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/class/thermal/thermal_zone1/temp",
        "/sys/class/hwmon/hwmon0/temp1_input",
        "/sys/class/hwmon/hwmon1/temp1_input",
        "/sys/class/hwmon/hwmon2/temp1_input"
    };

    for (const auto& path : thermalPaths) {
        ifstream file(path);
        if (file.is_open()) {
            long rawTemp = 0;
            if (file >> rawTemp && rawTemp > 0) {
                info.cpuTempC = rawTemp / 1000.0;
                info.isAvailable = true;
                info.label = "CPU Temp";
                return info;
            }
        }
    }

    return info;
}
