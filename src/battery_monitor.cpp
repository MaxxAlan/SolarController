#include "battery_monitor.h"

BatteryMonitor::BatteryMonitor()
    : voltage(3.8f), socPercent(80), lowBatteryDetected(false), chargingState(false), lastCheckMs(0) {}

void BatteryMonitor::begin() {
#if ENABLE_BATTERY_SENSE
    pinMode(PIN_BATTERY_ADC, INPUT);
    analogReadResolution(12);
#endif
    update();
}

float BatteryMonitor::readAveragedADC(int pin, int samples) {
    long sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(pin);
        delayMicroseconds(200);
    }
    return (float)sum / (float)samples;
}

int BatteryMonitor::calculateSOC(float v) {
    // Ước lượng SOC (%) cho cell Li-ion 18650 theo đường đặc tính xả điện áp
    if (v >= 4.18f) return 100;
    if (v >= 4.05f) return 90;
    if (v >= 3.95f) return 80;
    if (v >= 3.85f) return 70;
    if (v >= 3.78f) return 60;
    if (v >= 3.73f) return 50;
    if (v >= 3.68f) return 40;
    if (v >= 3.60f) return 30;
    if (v >= 3.50f) return 20;
    if (v >= 3.40f) return 10;
    if (v >= 3.30f) return 5;
    return 0;
}

void BatteryMonitor::update() {
    unsigned long now = millis();
    if (now - lastCheckMs < 1000 && lastCheckMs != 0) {
        return;
    }
    lastCheckMs = now;

#if ENABLE_BATTERY_SENSE
    float rawAdc = readAveragedADC(PIN_BATTERY_ADC, 16);
    // Tính điện áp chân pin qua phân áp
    float pinVoltage = (rawAdc / ADC_RESOLUTION) * ADC_VREF;
    voltage = pinVoltage * VOLTAGE_DIVIDER_RATIO;

    // Giới hạn điện áp hợp lý tránh đọc trôi nổi
    if (voltage < 0.5f) {
        voltage = 0.0f;
    }

    socPercent = calculateSOC(voltage);

    if (voltage > 0.5f && voltage < BATTERY_LOW_VOLTAGE) {
        lowBatteryDetected = true;
    } else {
        lowBatteryDetected = false;
    }
#else
    // Khi chưa kích hoạt sensor phần cứng, cung cấp giá trị danh định an toàn
    voltage = 3.90f;
    socPercent = 75;
    lowBatteryDetected = false;
#endif
}
