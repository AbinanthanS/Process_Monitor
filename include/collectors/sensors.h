#ifndef SENSORS_H
#define SENSORS_H

#include <string>

struct SensorInfo {
    double cpuTempC = -1.0;
    bool isAvailable = false;
    std::string label;
};

SensorInfo readSensors();

#endif // SENSORS_H
