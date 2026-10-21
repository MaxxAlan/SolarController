#pragma once

#include <Arduino.h>
#include "config.h"

class BatteryMonitor {
public:
    BatteryMonitor();
    void begin();
    void update();

    float getVoltage() const { return voltage; }
    int getSOC() const { return socPercent; }
    bool isLowBattery() const { return lowBatteryDetected; }
    bool isCharging() const { return chargingState; }

private:
    float voltage;
    int socPercent;
    bool lowBatteryDetected;
    bool chargingState;
    unsigned long lastCheckMs;

    int calculateSOC(float v);
    float readAveragedADC(int pin, int samples = 10);
};
