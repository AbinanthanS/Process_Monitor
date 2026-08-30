#include "sensors.h"
#include <fstream>
#include <dirent.h>
#include <string>

using namespace std;

SensorInfo readSensors() {
    SensorInfo info{};

    // 1. Try /sys/class/thermal/thermal_zone0/temp
    for (int zone = 0; zone < 5; ++zone) {
        string path = "/sys/class/thermal/thermal_zone" + to_string(zone) + "/temp";
        ifstream file(path);
        if (file.is_open()) {
            long milliC = 0;
            if (file >> milliC && milliC > 0) {
                info.cpuTempC = milliC / 1000.0;
                info.isAvailable = true;
                info.label = "Zone " + to_string(zone);
                return info;
            }
        }
    }

    // 2. Try /sys/class/hwmon/hwmon0/temp1_input
    for (int hw = 0; hw < 5; ++hw) {
        string path = "/sys/class/hwmon/hwmon" + to_string(hw) + "/temp1_input";
        ifstream file(path);
        if (file.is_open()) {
            long milliC = 0;
            if (file >> milliC && milliC > 0) {
                info.cpuTempC = milliC / 1000.0;
                info.isAvailable = true;
                info.label = "hwmon" + to_string(hw);
                return info;
            }
        }
    }

    return info;
}
