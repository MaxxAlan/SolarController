#include <Arduino.h>
#include "config.h"
#include "tracker.h"
#include "battery_monitor.h"
#include "network_manager.h"

// Khởi tạo các đối tượng điều khiển hệ thống
SolarTracker tracker;
BatteryMonitor battery;
SolarNetworkManager network(tracker, battery);

unsigned long lastSerialLogMs = 0;

void printBanner() {
    Serial.println("\n=======================================================");
    Serial.println("   ☀️ SolarController - Dual/Single-Axis Solar Tracker   ");
    Serial.println("   Vi điều khiển: ESP32-S3-WROOM-1 N16R8 (Octal SPI)   ");
    Serial.println("   Lưu đồ thuật toán: Theo FlowChart v2 (Draw.io)       ");
    Serial.println("=======================================================");
}

void setup() {
    Serial.begin(115200);
    delay(1000); // Đợi ổn định nguồn và Serial CDC

    printBanner();

    // 1. Khởi tạo thuật toán bám sáng & cảm biến quang học
    Serial.println("[INIT] Step 1/3: Initializing Solar Tracker & Servos...");
    tracker.begin();

    // 2. Khởi tạo module giám sát pin 18650 & sạc
    Serial.println("[INIT] Step 2/3: Initializing Battery Monitor...");
    battery.begin();

    // 3. Khởi tạo mạng (Wi-Fi, Web Dashboard, Firebase)
    // Tự động chuyển sang Offline Failsafe Mode nếu không kết nối được Wi-Fi
    Serial.println("[INIT] Step 3/3: Initializing Network & IoT Telemetry...");
    network.begin();

    Serial.println("[SYSTEM] Ready! Entering main loop...\n");
}

void loop() {
    // 1. Cập nhật tác vụ mạng (Web server, Firebase, nhận lệnh Dashboard)
    network.update();

    // 2. Giám sát pin và kiểm tra bảo vệ quá xả / sạc
    battery.update();

    // 3. Xử lý thuật toán bám sáng (Deadband, Night Mode, Sun Tracking, Safe Mode)
    tracker.update(battery.isLowBattery());

    // 4. In thông số Serial định kỳ để theo dõi trực tiếp
    unsigned long now = millis();
    if (now - lastSerialLogMs >= 2000) {
        lastSerialLogMs = now;
        LDRReading ldr = tracker.getLDRData();

        Serial.printf(
            "[STATUS] Mode: %s | Angle: %3d° (Tgt: %3d°) | LDR [TL:%4d TR:%4d BL:%4d BR:%4d] Diff: %4d | Bat: %.2fV (%d%%) | Net: %s\n",
            tracker.getModeString(),
            tracker.getCurrentAngle(),
            tracker.getTargetAngle(),
            ldr.topLeft,
            ldr.topRight,
            ldr.bottomLeft,
            ldr.bottomRight,
            ldr.diff,
            battery.getVoltage(),
            battery.getSOC(),
            network.isOnline() ? "ONLINE" : "OFFLINE"
        );
    }

    // Độ trễ nhỏ để nhường CPU cho các tác vụ ngầm của ESP32 (Wi-Fi & Watchdog)
    delay(10);
}
