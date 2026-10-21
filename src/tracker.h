#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"

enum SystemMode {
    MODE_AUTO = 0,
    MODE_MANUAL = 1,
    MODE_NIGHT = 2,
    MODE_FAULT = 3
};

struct LDRReading {
    int topLeft;
    int topRight;
    int bottomLeft;
    int bottomRight;
    int leftAvg;
    int rightAvg;
    int totalAvg;
    int diff;
};

class SolarTracker {
public:
    SolarTracker();
    void begin();
    void update(bool lowBattery = false);

    // Điều khiển chế độ
    void setMode(SystemMode mode);
    SystemMode getMode() const { return currentMode; }
    const char* getModeString() const;

    // Điều khiển thủ công
    void setManualAngle(int angle);
    int getTargetAngle() const { return targetAngle; }
    int getCurrentAngle() const { return currentAngle; }

    // Dữ liệu cảm biến
    LDRReading getLDRData() const { return ldrData; }

    // Trạng thái lỗi
    bool isFault() const { return faultActive; }
    const char* getFaultReason() const { return faultReason; }
    void clearFault();

private:
    Servo servoMotor;
    SystemMode currentMode;
    SystemMode userSelectedMode; // Lưu chế độ người dùng chọn (AUTO hoặc MANUAL)

    LDRReading ldrData;
    int currentAngle;
    int targetAngle;

    bool faultActive;
    const char* faultReason;
    unsigned long lastTrackingUpdateMs;

    void readLDRs();
    void applyServoAngle(int angle);
    void handleAutoTracking();
    void handleManualTracking();
    void handleNightMode();
    void handleFaultState();
};
