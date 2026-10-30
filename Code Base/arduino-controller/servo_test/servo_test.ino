/* =============================================================================
 *  SolarController - SERVO TEST FIRMWARE
 *  Board : ESP32-S3 DevKit (N16R8) - Arduino IDE
 *  Mục đích: Kiểm tra servo SG90 (cắt trục → position control 0°-360°)
 *            TRƯỚC khi nối vào hệ thống chính.
 *
 *  Đấu chân:
 *      Servo Signal  -> GPIO 18
 *      Servo VCC     -> 5V (NGUỒN NGOÀI, KHÔNG dùng 3V3 của ESP32)
 *      Servo GND     -> GND chung
 *
 *  La bàn (nhìn từ trên, chiều kim đồng hồ):
 *      0° = Bắc  |  90° = Đông  |  180° = Nam  |  270° = Tây
 *
 *  Lệnh Serial (115200, Enter để gửi):
 *      h        : trợ giúp
 *      <số>     : quay đến góc (0-360)
 *      s        : quét thử 0° → 360° → 90° (East park)
 *      e        : quay về hướng Đông (90°)
 *      n        : quay về hướng Nam (180°)
 *      w        : quay về hướng Tây (270°)
 *      b        : quay về hướng Bắc (0°)
 *      +/-      : tăng/giảm 5°
 * ============================================================================= */

#include <ESP32Servo.h>

Servo servoSG90;

#define SERVO_PIN     18
#define SERVO_MIN_US  500
#define SERVO_MAX_US  2400

int currentAngle = 90; // khởi tại hướng Đông
String serialBuf = "";

void printHelp() {
    Serial.println();
    Serial.println("=== SERVO TEST ===");
    Serial.printf("Pin = GPIO%d | PWM: %d-%d us\n", SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
    Serial.printf("Goc hien tai: %d°\n", currentAngle);
    Serial.println("--- LENH (ket thuc Enter) ---");
    Serial.println(" h       : tro giup");
    Serial.println(" <0-360> : quay den goc");
    Serial.println(" s       : quet thu 0->360->90 (sweep test)");
    Serial.println(" e       : Dong (90)  n : Nam (180)");
    Serial.println(" w       : Tay (270)  b : Bac (0)");
    Serial.println(" + / -   : +5 / -5 do");
    Serial.println("-----------------------------");
}

void setAngle(int angle) {
    currentAngle = constrain(angle, 0, 360);
    servoSG90.write(currentAngle);
    Serial.printf("[SERVO] Goc = %d°\n", currentAngle);
}

void sweepTest() {
    Serial.println("[SWEEP] Bat dau quet: 0 -> 360 -> 90");
    for (int a = 0; a <= 360; a += 5) {
        servoSG90.write(a);
        Serial.printf("  -> %d°\n", a);
        delay(80);
    }
    delay(500);
    for (int a = 360; a >= 90; a -= 5) {
        servoSG90.write(a);
        Serial.printf("  <- %d°\n", a);
        delay(80);
    }
    currentAngle = 90;
    Serial.println("[SWEEP] Hoan tat. Ve vi tri Dong (90°).");
}

void processCommand(String line) {
    line.trim();
    if (line.length() == 0) return;

    String lower = line;
    lower.toLowerCase();

    if (lower == "h" || lower == "help") {
        printHelp();
    } else if (lower == "s" || lower == "sweep") {
        sweepTest();
    } else if (lower == "e" || lower == "east") {
        setAngle(90);
    } else if (lower == "n" || lower == "south") {
        setAngle(180);
    } else if (lower == "w" || lower == "west") {
        setAngle(270);
    } else if (lower == "b" || lower == "north") {
        setAngle(0);
    } else if (lower == "+") {
        setAngle(currentAngle + 5);
    } else if (lower == "-") {
        setAngle(currentAngle - 5);
    } else {
        // Thử parse số
        int val = lower.toInt();
        if (val > 0 || lower == "0") {
            setAngle(val);
        } else {
            Serial.printf("[CMD] Khong hieu: %s\n", line.c_str());
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("=========================================");
    Serial.println("  SolarController - SERVO TEST");
    Serial.println("  SG90 cat truc (0-360 position control)");
    Serial.println("=========================================");

    ESP32PWM::allocateTimer(0);
    servoSG90.setPeriodHertz(50);
    servoSG90.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
    setAngle(90); // khởi tại Đông

    printHelp();
}

void loop() {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            processCommand(serialBuf);
            serialBuf = "";
        } else if (serialBuf.length() < 64) {
            serialBuf += c;
        }
    }
    delay(5);
}