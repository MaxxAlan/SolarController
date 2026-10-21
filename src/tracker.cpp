#include "tracker.h"

SolarTracker::SolarTracker()
    : currentMode(MODE_AUTO),
      userSelectedMode(MODE_AUTO),
      currentAngle(SERVO_INITIAL_ANGLE),
      targetAngle(SERVO_INITIAL_ANGLE),
      faultActive(false),
      faultReason("None"),
      lastTrackingUpdateMs(0) {
    memset(&ldrData, 0, sizeof(ldrData));
}

void SolarTracker::begin() {
    // Cấu hình các chân cảm biến quang trở (ADC1)
    pinMode(PIN_LDR_TOP_LEFT, INPUT);
    pinMode(PIN_LDR_TOP_RIGHT, INPUT);
    pinMode(PIN_LDR_BOTTOM_LEFT, INPUT);
    pinMode(PIN_LDR_BOTTOM_RIGHT, INPUT);

    // Cấu hình buzzer cảnh báo nếu có
    pinMode(PIN_BUZZER_ALARM, OUTPUT);
    digitalWrite(PIN_BUZZER_ALARM, LOW);

    // Cấu hình Servo PWM (ESP32Servo)
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    servoMotor.setPeriodHertz(50); // Standard 50Hz servo
    servoMotor.attach(PIN_SERVO_HORIZONTAL, 500, 2500);

    // Đưa servo về góc mặc định ban đầu
    currentAngle = constrain(SERVO_INITIAL_ANGLE, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    targetAngle = currentAngle;
    servoMotor.write(currentAngle);

    Serial.printf("[TRACKER] Initialized on GPIO %d, initial angle: %d deg\n", PIN_SERVO_HORIZONTAL, currentAngle);
}

void SolarTracker::readLDRs() {
    // Đọc trung bình mẫu để chống nhiễu ADC
    const int SAMPLES = 8;
    long tl = 0, tr = 0, bl = 0, br = 0;

    for (int i = 0; i < SAMPLES; i++) {
        tl += analogRead(PIN_LDR_TOP_LEFT);
        tr += analogRead(PIN_LDR_TOP_RIGHT);
        bl += analogRead(PIN_LDR_BOTTOM_LEFT);
        br += analogRead(PIN_LDR_BOTTOM_RIGHT);
        delayMicroseconds(100);
    }

    ldrData.topLeft     = tl / SAMPLES;
    ldrData.topRight    = tr / SAMPLES;
    ldrData.bottomLeft  = bl / SAMPLES;
    ldrData.bottomRight = br / SAMPLES;

    ldrData.leftAvg  = (ldrData.topLeft + ldrData.bottomLeft) / 2;
    ldrData.rightAvg = (ldrData.topRight + ldrData.bottomRight) / 2;
    ldrData.totalAvg = (ldrData.leftAvg + ldrData.rightAvg) / 2;
    ldrData.diff     = ldrData.leftAvg - ldrData.rightAvg;
}

void SolarTracker::applyServoAngle(int angle) {
    int clamped = constrain(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    if (clamped != currentAngle) {
        currentAngle = clamped;
        servoMotor.write(currentAngle);
    }
}

void SolarTracker::setMode(SystemMode mode) {
    if (mode == MODE_AUTO || mode == MODE_MANUAL) {
        userSelectedMode = mode;
    }
    currentMode = mode;
    if (currentMode != MODE_FAULT) {
        faultActive = false;
        digitalWrite(PIN_BUZZER_ALARM, LOW);
    }
    Serial.printf("[TRACKER] Mode changed to: %s\n", getModeString());
}

void SolarTracker::setManualAngle(int angle) {
    targetAngle = constrain(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
}

void SolarTracker::clearFault() {
    faultActive = false;
    faultReason = "None";
    digitalWrite(PIN_BUZZER_ALARM, LOW);
    currentMode = userSelectedMode;
}

const char* SolarTracker::getModeString() const {
    switch (currentMode) {
        case MODE_AUTO:   return "AUTO";
        case MODE_MANUAL: return "MANUAL";
        case MODE_NIGHT:  return "NIGHT";
        case MODE_FAULT:  return "FAULT";
        default:          return "UNKNOWN";
    }
}

void SolarTracker::handleFaultState() {
    // Dừng motor hoàn toàn, phát chuông cảnh báo
    digitalWrite(PIN_BUZZER_ALARM, HIGH);
}

void SolarTracker::handleNightMode() {
    // Trả tấm pin về hướng Đông (SERVO_EAST_PARK_ANGLE) để đón sáng sớm mai
    targetAngle = SERVO_EAST_PARK_ANGLE;
    if (currentAngle > targetAngle) {
        applyServoAngle(currentAngle - SERVO_STEP_SIZE);
    } else if (currentAngle < targetAngle) {
        applyServoAngle(currentAngle + SERVO_STEP_SIZE);
    }
}

void SolarTracker::handleManualTracking() {
    // Di chuyển dần mượt mà tới góc do Dashboard yêu cầu
    if (currentAngle < targetAngle) {
        applyServoAngle(min(currentAngle + SERVO_STEP_SIZE, targetAngle));
    } else if (currentAngle > targetAngle) {
        applyServoAngle(max(currentAngle - SERVO_STEP_SIZE, targetAngle));
    }
}

void SolarTracker::handleAutoTracking() {
    // 1. Kiểm tra trời tối (Night Mode)
    if (ldrData.totalAvg < NIGHT_THRESHOLD) {
        currentMode = MODE_NIGHT;
        handleNightMode();
        return;
    }

    // 2. Kiểm tra ngưỡng sai số (Deadband Threshold)
    if (abs(ldrData.diff) < DEADBAND_THRESHOLD) {
        // Ánh sáng 2 bên cân bằng: Giữ nguyên góc hiện tại, motor nghỉ
        return;
    }

    // 3. Tự động xoay về bên sáng hơn
    // Lưu ý: Tuỳ theo chiều lắp đặt cơ khí của servo, nếu diff > 0 (bên trái sáng hơn):
    // Cần tăng hoặc giảm góc để hướng tấm pin về phía đó.
    if (ldrData.diff > DEADBAND_THRESHOLD) {
        // Phía trái sáng hơn => di chuyển góc hướng về bên trái
        targetAngle = constrain(currentAngle + SERVO_STEP_SIZE, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    } else if (ldrData.diff < -DEADBAND_THRESHOLD) {
        // Phía phải sáng hơn => di chuyển góc hướng về bên phải
        targetAngle = constrain(currentAngle - SERVO_STEP_SIZE, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    }

    applyServoAngle(targetAngle);
}

void SolarTracker::update(bool lowBattery) {
    // 1. Đọc cảm biến LDR
    readLDRs();

    // 2. Kiểm tra điều kiện lỗi (Low battery hoặc đứt cảm biến)
    if (lowBattery) {
        faultActive = true;
        faultReason = "Low Battery Voltage";
        currentMode = MODE_FAULT;
    }

    // 3. Điều phối theo Flowchart State Machine
    unsigned long now = millis();
    if (now - lastTrackingUpdateMs < TRACKING_INTERVAL_MS) {
        return;
    }
    lastTrackingUpdateMs = now;

    if (currentMode == MODE_FAULT) {
        handleFaultState();
        return;
    }

    if (userSelectedMode == MODE_MANUAL) {
        currentMode = MODE_MANUAL;
        handleManualTracking();
        return;
    }

    // Nếu ở chế độ AUTO
    if (currentMode == MODE_NIGHT) {
        // Kiểm tra xem đã sáng lại chưa
        if (ldrData.totalAvg >= NIGHT_THRESHOLD + 40) { // Có hysteresis chống chập chờn
            currentMode = MODE_AUTO;
        } else {
            handleNightMode();
            return;
        }
    }

    handleAutoTracking();
}
