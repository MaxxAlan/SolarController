/* =============================================================================
 *  SolarController - LDR MODULE TEST FIRMWARE  (v1.0.0)
 *  -----------------------------------------------------------------------------
 *  Board : ESP32-S3 DevKit (N16R8) - Arduino IDE
 *  Mục đích: kiểm tra 4 module quang trở LM393 (AO + DO) TRƯỚC khi nối vào
 *            hệ thống bám sáng đầy đủ. Không điều khiển servo, không đụng pin.
 *
 *  Đấu chân (module LDR LM393):
 *      TL (trên-trái)    AO -> GPIO 4 (ADC1_CH3)   DO -> GPIO 15
 *      TR (trên-phải)    AO -> GPIO 5 (ADC1_CH4)   DO -> GPIO 16
 *      BL (dưới-trái)   AO -> GPIO 6 (ADC1_CH5)   DO -> GPIO 17
 *      BR (dưới-phải)   AO -> GPIO 7 (ADC1_CH6)   DO -> GPIO 18
 *      Tất cả module: VCC -> 3V3, GND -> GND
 *
 *  Lệnh Serial (mở Serial Monitor 115200):
 *      h  - trợ giúp          c  - chạy calibration min/max (10s)
 *      r  - reset thống kê    R  - xoá kết quả calibration
 *      l  - bật/tắt ghi CSV   f  - bật/tắt gửi Firebase
 *      x  - đổi hiển thị raw/normalized     p - in cấu hình + kết quả calib
 *      e  - xuất hằng số C++ để copy vào include/config.h
 * ============================================================================= */

#include <Arduino.h>
#include <WiFi.h>

// ==============================================================================
// 1. CẤU HÌNH - SỬA Ở ĐÂY
// ==============================================================================

#define FW_VERSION "1.0.0"

// --- Pin analog LDR (ADC1: hoạt động ổn định kể cả khi đã bật Wi-Fi) ---
#define PIN_LDR_TL 4  // ADC1_CH3
#define PIN_LDR_TR 5  // ADC1_CH4
#define PIN_LDR_BL 6  // ADC1_CH5
#define PIN_LDR_BR 7  // ADC1_CH6

// --- Pin digital LM393 (DO). Đặt -1 nếu chưa nối chân DO ---
#define USE_DO_OUTPUTS 1
#define PIN_DO_TL 15
#define PIN_DO_TR 16
#define PIN_DO_BL 17
#define PIN_DO_BR 18

// LM393: DO thường LOW khi đủ sáng -> quy đổi "sáng" = !LOW
#define DO_ACTIVE_LOW 1

// --- ADC ---
#define ADC_FULL_SCALE 4095
#define ADC_VREF_MV 3300
#define ADC_SAMPLES 16
#define ADC_SAMPLE_GAP_US 200

// --- Chu kỳ (ms) ---
#define DISPLAY_INTERVAL_MS 500
#define CSV_INTERVAL_MS 200
#define SELFTEST_INTERVAL_MS 5000
#define FIREBASE_INTERVAL_MS 2000
#define CALIBRATION_MS 10000
#define WIFI_CONNECT_TIMEOUT_MS 12000

// --- Ngưỡng thuật toán (đồng bộ include/config.h) ---
#define NIGHT_THRESHOLD 250
#define DEADBAND_THRESHOLD 60

// --- Wi-Fi ---
#define WIFI_SSID "hoangmanh"
#define WIFI_PASSWORD "00000000"

// --- Firebase (bật = 1 thì cài lib "Firebase Arduino Client Library for
//     ESP8266 and ESP32" + "ArduinoJson v7" trong Library Manager) ---
#define ENABLE_FIREBASE 0
#if ENABLE_FIREBASE
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>
#define FIREBASE_HOST                                                          \
  "https://solarcontroller-1cc93-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "XURINmjqSrUgYAs1B7TjQbZoe8Nn151O8BWGRKYs"
// Ghi đè telemetry thật của firmware chính -> dùng để xem Dashboard
#define FIREBASE_PATH "/solar_controller/telemetry"
#endif

// ==============================================================================
// 2. BIẾN & KIỂU DỮ LIỆU
// ==============================================================================

enum ChIdx { CH_TL = 0, CH_TR = 1, CH_BL = 2, CH_BR = 3, CH_COUNT = 4 };

struct LdrChannel {
    const char *name;
    uint8_t pin;
    int8_t doPin;
    int raw;         // giá trị ADC 0..4095
    int mv;          // milivolt
    int pct;         // % full scale
    int prevRaw;     // giá trị trước (phát hiện kẹt)
    bool doRaw;      // mức logic thô của chân DO
    bool doBright;   // diễn giải theo DO_ACTIVE_LOW
    bool doChanged;  // DO có đổi trạng thái lần gần nhất không
    bool doToggled;  // DO có đổi trạng thái trong lúc calibration không
    int minRaw;      // min kể từ lần reset
    int maxRaw;      // max kể từ lần reset
    int calMin;      // min của lần calibration
    int calMax;      // max của lần calibration
    int stuckLow;    // số lần đọc liên tiếp ra ~0
    int stuckHigh;   // số lần đọc liên tiếp ra ~4095
    unsigned long lastChangeMs;
};

LdrChannel channels[CH_COUNT] = {
    {"TL", PIN_LDR_TL, PIN_DO_TL, 0, 0, 0, -1, false, false, false, false, ADC_FULL_SCALE, 0, -1, -1, 0, 0, 0},
    {"TR", PIN_LDR_TR, PIN_DO_TR, 0, 0, 0, -1, false, false, false, false, ADC_FULL_SCALE, 0, -1, -1, 0, 0, 0},
    {"BL", PIN_LDR_BL, PIN_DO_BL, 0, 0, 0, -1, false, false, false, false, ADC_FULL_SCALE, 0, -1, -1, 0, 0, 0},
    {"BR", PIN_LDR_BR, PIN_DO_BR, 0, 0, 0, -1, false, false, false, false, ADC_FULL_SCALE, 0, -1, -1, 0, 0, 0},
};

bool csvEnabled = false;
bool showNormalized = false;
bool calibrating = false;
bool calibDone = false;
unsigned long calibStartMs = 0;
unsigned long lastDisplayMs = 0;
unsigned long lastCsvMs = 0;
unsigned long lastSelfTestMs = 0;

#if ENABLE_FIREBASE
bool firebaseReady = false;
bool firebaseEnabled = true;
unsigned long lastFirebaseMs = 0;
FirebaseData fbdo;
FirebaseConfig fbConfig;
Auth auth;
#endif

// ==============================================================================
// 3. TIỆN ÍCH
// ==============================================================================

int leftAvg() { return (channels[CH_TL].raw + channels[CH_BL].raw) / 2; }
int rightAvg() { return (channels[CH_TR].raw + channels[CH_BR].raw) / 2; }
int totalAvg() { return (leftAvg() + rightAvg()) / 2; }
int diffVal() { return leftAvg() - rightAvg(); }

// Giá trị chuẩn hoá 0..1000 (dùng calibration nếu có, không thì theo full scale)
int normalized(int i) {
    const LdrChannel &c = channels[i];
    if (c.calMax > c.calMin) {
        long v = (long)(c.raw - c.calMin) * 1000 / (c.calMax - c.calMin);
        return (int)constrain(v, 0L, 1000L);
    }
    return (int)constrain((long)c.raw * 1000 / ADC_FULL_SCALE, 0L, 1000L);
}

void printBar(int value, int maxValue, int width) {
    int filled = (int)((long)value * width / (maxValue > 0 ? maxValue : 1));
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;
    for (int i = 0; i < width; i++) Serial.print(i < filled ? '#' : '-');
}

const char *dayNightState(int total) {
    return total < NIGHT_THRESHOLD ? "NIGHT" : "DAY";
}

const char *servoAction(int diff) {
    if (totalAvg() < NIGHT_THRESHOLD) return "PARK (ngoai dong)";
    if (diff > DEADBAND_THRESHOLD) return "XOAY TRAI";
    if (diff < -DEADBAND_THRESHOLD) return "XOAY PHAI";
    return "GIU (can bang)";
}

const char *brighterSide(int diff) {
    if (diff > DEADBAND_THRESHOLD) return "TRAI SANG HON";
    if (diff < -DEADBAND_THRESHOLD) return "PHAI SANG HON";
    return "CAN BANG";
}

// ==============================================================================
// 4. ĐỌC CẢM BIẾN
// ==============================================================================

void readAllSensors() {
    for (int i = 0; i < CH_COUNT; i++) {
        LdrChannel &c = channels[i];

        long sum = 0;
        for (int s = 0; s < ADC_SAMPLES; s++) {
            sum += analogRead(c.pin);
            delayMicroseconds(ADC_SAMPLE_GAP_US);
        }
        c.raw = (int)(sum / ADC_SAMPLES);
        c.mv = (int)((long)c.raw * ADC_VREF_MV / ADC_FULL_SCALE);
        c.pct = (int)((long)c.raw * 100 / ADC_FULL_SCALE);

        if (c.raw != c.prevRaw) {
            c.prevRaw = c.raw;
            c.lastChangeMs = millis();
        }
        if (c.raw <= 1) c.stuckLow++; else c.stuckLow = 0;
        if (c.raw >= ADC_FULL_SCALE - 1) c.stuckHigh++; else c.stuckHigh = 0;
        if (c.raw < c.minRaw) c.minRaw = c.raw;
        if (c.raw > c.maxRaw) c.maxRaw = c.raw;

#if USE_DO_OUTPUTS
        if (c.doPin >= 0) {
            bool level = digitalRead(c.doPin);
            if (level != c.doRaw) {
                c.doChanged = true;
                if (calibrating) c.doToggled = true;
            }
            c.doRaw = level;
            c.doBright = DO_ACTIVE_LOW ? (!level) : level;
        }
#endif

        // Thu thập min/max trong lúc calibration
        if (calibrating) {
            if (c.raw < c.calMin) c.calMin = c.raw;
            if (c.raw > c.calMax) c.calMax = c.raw;
        }
    }
}

// ==============================================================================
// 5. IN BẢNG CONSOLE
// ==============================================================================

void printBanner() {
    Serial.println();
    Serial.println("=========================================================");
    Serial.println("   SolarController - LDR MODULE TEST  v" FW_VERSION);
    Serial.println("   ESP32-S3 | 4x LDR LM393 (AO + DO)");
    Serial.println("=========================================================");
    Serial.println("[PIN] TL: AO=GPIO4  DO=GPIO15");
    Serial.println("[PIN] TR: AO=GPIO5  DO=GPIO16");
    Serial.println("[PIN] BL: AO=GPIO6  DO=GPIO17");
    Serial.println("[PIN] BR: AO=GPIO7  DO=GPIO18");
    Serial.println("[NGUONG] NIGHT=" NIGHT_THRESHOLD "  DEADBAND=" DEADBAND_THRESHOLD);
    Serial.println();
}

void printTable() {
    Serial.println();
    Serial.printf("%-4s %-6s %6s %5s  %-14s %-14s %s\n", "CH", "PIN", "RAW", "%", "LEVEL", "DO", "CAL(min..max)");
    Serial.println("---------------------------------------------------------");
    for (int i = 0; i < CH_COUNT; i++) {
        LdrChannel &c = channels[i];
        Serial.printf("%-4s GPIO%-3d %6d %4d%%  ", c.name, c.pin, c.raw, c.pct);
        printBar(c.raw, ADC_FULL_SCALE, 14);
#if USE_DO_OUTPUTS
        if (c.doPin >= 0) {
            Serial.printf("  %-4s(%s)", c.doRaw ? "HIGH" : "LOW", c.doBright ? "SANG" : "TOI");
        } else {
            Serial.printf("  %-14s", "N/A");
        }
#else
        Serial.printf("  %-14s", "N/A");
#endif
        if (c.calMax > c.calMin) {
            Serial.printf("  %4d..%-4d", c.calMin, c.calMax);
        } else {
            Serial.printf("  %-9s", "chua calib");
        }
        Serial.println();
    }
    Serial.println("---------------------------------------------------------");
    Serial.printf("TOTAL=%d (%d%%)  LEFT=%d  RIGHT=%d  DIFF=%+d  -> %s\n", totalAvg(),
                  (int)((long)totalAvg() * 100 / ADC_FULL_SCALE), leftAvg(), rightAvg(), diffVal(),
                  brighterSide(diffVal()));
    Serial.printf("STATE=%s  |  SERVO: %s\n", dayNightState(totalAvg()), servoAction(diffVal()));
    if (showNormalized) {
        Serial.printf("NORM: TL=%d TR=%d BL=%d BR=%d\n", normalized(CH_TL), normalized(CH_TR),
                      normalized(CH_BL), normalized(CH_BR));
    }
}

void printCsv() {
    Serial.printf("%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", millis(), channels[CH_TL].raw,
                  channels[CH_TR].raw, channels[CH_BL].raw, channels[CH_BR].raw, diffVal(), totalAvg(),
                  normalized(CH_TL), normalized(CH_TR), normalized(CH_BL), normalized(CH_BR),
                  channels[CH_TL].doRaw ? 1 : 0, channels[CH_TR].doRaw ? 1 : 0, channels[CH_BL].doRaw ? 1 : 0,
                  channels[CH_BR].doRaw ? 1 : 0);
}

// ==============================================================================
// 6. SELF-TEST KIỂM TRA CHÂN / DÂY
// ==============================================================================

void printSelfTest() {
    bool anyIssue = false;
    unsigned long now = millis();

    Serial.println();
    Serial.println("=== SELF-TEST ===");

    for (int i = 0; i < CH_COUNT; i++) {
        LdrChannel &c = channels[i];
        String status = "OK";
        String note = "";

        if (c.stuckLow > 200) {
            status = "LO";
            note = "AO luon ~0 -> module chua cap nguon / kẹt GND / sai chan ADC";
            anyIssue = true;
        } else if (c.stuckHigh > 200) {
            status = "HI";
            note = "AO luon ~4095 -> chan AO khong noi / module khong noi GND";
            anyIssue = true;
        } else if (now - c.lastChangeMs > 20000) {
            status = "STATIC";
            note = "gia tri khong doi 20s -> LDR hong / ket noi sai / bien tro cua module";
            anyIssue = true;
        }

        Serial.printf("%-4s GPIO%-3d %-7s %s\n", c.name, c.pin, status.c_str(), note.c_str());
    }

    // 4 kênh bằng nhau tuyệt đối -> nghi ngờ đấu nhầm dây
    bool allSame = true;
    for (int i = 1; i < CH_COUNT; i++) {
        if (abs(channels[i].raw - channels[0].raw) > 5) allSame = false;
    }
    if (allSame) {
        Serial.println("WARN  4 kenh gia tri giong nhau -> kiem tra lai duong day AO");
        anyIssue = true;
    }

#if USE_DO_OUTPUTS
    for (int i = 0; i < CH_COUNT; i++) {
        LdrChannel &c = channels[i];
        if (c.doPin >= 0 && calibDone && !c.doToggled) {
            Serial.printf("WARN  %-4s DO (GPIO%d) khong doi trong luc calib -> nguong bien tro qua cao/thap, hoac DO khong noi\n",
                          c.name, c.doPin);
            anyIssue = true;
        }
    }
#endif

    if (!anyIssue) Serial.println("OK    Tat ca kenh va chan DO hoat dong binh thuong.");
    Serial.println("=== HET SELF-TEST ===");
}

// ==============================================================================
// 7. CALIBRATION MIN/MAX
// ==============================================================================

void startCalibration() {
    calibrating = true;
    calibDone = false;
    calibStartMs = millis();
    for (int i = 0; i < CH_COUNT; i++) {
        channels[i].calMin = ADC_FULL_SCALE;
        channels[i].calMax = 0;
        channels[i].doChanged = false;
        channels[i].doToggled = false;
    }
    Serial.println();
    Serial.println("[CAL] Bat dau capture 10 giay.");
    Serial.println("[CAL] Che TAT CA 4 LDR trong 5s dau (lay gia tri toi),");
    Serial.println("[CAL] sau do tha ra, huong den nang (lay gia tri sang) 5s cuoi.");
}

void updateCalibration(unsigned long now) {
    if (!calibrating) return;
    if (now - calibStartMs >= CALIBRATION_MS) {
        calibrating = false;
        printCalibrationReport();
        return;
    }
    static unsigned long lastTick = 0;
    if (now - lastTick >= 1000) {
        lastTick = now;
        int remain = (CALIBRATION_MS - (now - calibStartMs)) / 1000;
        Serial.printf("[CAL] con %d giay...\n", remain);
    }
}

void printCalibrationReport() {
    calibDone = true;
    Serial.println();
    Serial.println("=== KET QUA CALIBRATION ===");
    Serial.printf("%-4s %8s %8s %8s %10s  %s\n", "CH", "MIN", "MAX", "SPAN", "NORM(0-1000)", "DO TOGGLE");
    for (int i = 0; i < CH_COUNT; i++) {
        LdrChannel &c = channels[i];
        int span = c.calMax - c.calMin;
#if USE_DO_OUTPUTS
        const char *doInfo = (c.doPin < 0) ? "N/A" : (c.doToggled ? "CO" : "KHONG");
#else
        const char *doInfo = "N/A";
#endif
        Serial.printf("%-4s %8d %8d %8d %10d  %s\n", c.name, c.calMin, c.calMax, span, normalized(i), doInfo);
    }

    // Đề xuất ngưỡng cho config.h
    long spanSum = 0, darkSum = 0;
    for (int i = 0; i < CH_COUNT; i++) {
        spanSum += (channels[i].calMax - channels[i].calMin);
        darkSum += channels[i].calMin;
    }
    int avgSpan = (int)(spanSum / CH_COUNT);
    int avgDark = (int)(darkSum / CH_COUNT);
    int suggestNight = avgDark + avgSpan * 5 / 100;
    int suggestDeadband = max(20, avgSpan * 5 / 100);

    Serial.println();
    Serial.printf("DEADBAND  %d  ->  de xuat NIGHT_THRESHOLD = %d\n", DEADBAND_THRESHOLD, suggestNight);
    Serial.printf("DEADBAND  %d  ->  de xuat DEADBAND_THRESHOLD = %d\n", DEADBAND_THRESHOLD, suggestDeadband);
    Serial.println("[LUU Y] Neu gia tri toi cua module > 0, hay dua cam bien ve nguong 0 hoac dung nguong toi thuc te.");
    Serial.println("[LUU Y] Neu bien do AO qua hep, tang bien tro cua module de do phan giai tot hon.");
}

// Xuất hằng số C++ để copy vào include/config.h
void exportConstants() {
    if (!calibDone) {
        Serial.println("[EXPORT] Chua co ket qua calibration. Hay nhan 'c' truoc.");
        return;
    }
    long spanSum = 0, darkSum = 0;
    for (int i = 0; i < CH_COUNT; i++) {
        spanSum += (channels[i].calMax - channels[i].calMin);
        darkSum += channels[i].calMin;
    }
    int avgSpan = (int)(spanSum / CH_COUNT);
    int avgDark = (int)(darkSum / CH_COUNT);

    Serial.println();
    Serial.println("// ==== copy vao include/config.h ====");
    Serial.println("#define NIGHT_THRESHOLD " + String(avgDark + avgSpan * 5 / 100));
    Serial.println("#define DEADBAND_THRESHOLD " + String(max(20, avgSpan * 5 / 100)));
    Serial.println("// ==== het ====");
}

// ==============================================================================
// 8. FIREBASE (tuy chon)
// ==============================================================================

#if ENABLE_FIREBASE
void connectWifi() {
    Serial.print("[WIFI] Dang ket noi toi ");
    Serial.print(WIFI_SSID);
    Serial.print(" ... ");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_CONNECT_TIMEOUT_MS) {
        delay(200);
        Serial.print('.');
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[WIFI] OK - IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("[WIFI] FAIL - chi dung Serial (khong gui Firebase).");
    }
}

void initFirebase() {
    fbConfig.host = FIREBASE_HOST;
    fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&fbConfig, &auth);
    Firebase.reconnectWiFi(true);
    firebaseReady = true;
    Serial.println("[FIREBASE] Da khoi tao. Duong dan: " FIREBASE_PATH);
}

void sendFirebase() {
    if (!firebaseReady || !firebaseEnabled) return;
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[FIREBASE] Bo qua - WiFi chua ket noi.");
        return;
    }

    FirebaseJson json;
    json.set("ldr_tl", channels[CH_TL].raw);
    json.set("ldr_tr", channels[CH_TR].raw);
    json.set("ldr_bl", channels[CH_BL].raw);
    json.set("ldr_br", channels[CH_BR].raw);
    json.set("diff", diffVal());
    json.set("mode", "AUTO");
    json.set("angle", 90);
    json.set("test_mode", 1);
    json.set("uptime_s", (uint32_t)(millis() / 1000));

    if (!Firebase.RTDB.setJSON(&fbdo, FIREBASE_PATH, &json)) {
        Serial.printf("[FIREBASE] WRITE FAIL code=%d reason=%s\n", fbdo.errorCode(),
                      fbdo.errorReason().c_str());
    }
}
#endif

// ==============================================================================
// 9. LENH SERIAL
// ==============================================================================

void printHelp() {
    Serial.println();
    Serial.println("--- LENH SERIAL ---");
    Serial.println(" h : tro giup");
    Serial.println(" c : chay calibration min/max (10s)");
    Serial.println(" r : reset thong ke runtime");
    Serial.println(" R : xoa ket qua calibration");
    Serial.println(" l : bat/tat ghi CSV (copy vao Excel)");
    Serial.println(" x : doi hien thi raw / normalized");
    Serial.println(" p : in cau hinh + ket qua calibration");
    Serial.println(" e : xuat hang so C++ de copy vao config.h");
#if ENABLE_FIREBASE
    Serial.println(" f : bat/tat gui Firebase");
#endif
    Serial.println("--------------------");
}

void printConfigAndCalib() {
    Serial.println();
    Serial.println("=== CAU HINH ===");
    Serial.printf("FIRMWARE  : v%s\n", FW_VERSION);
    Serial.printf("ADC       : 12-bit, %d mau/do, Vref=%d mV\n", ADC_SAMPLES, ADC_VREF_MV);
    Serial.printf("NIGHT     : %d\n", NIGHT_THRESHOLD);
    Serial.printf("DEADBAND  : %d\n", DEADBAND_THRESHOLD);
    Serial.printf("CSV       : %s\n", csvEnabled ? "ON" : "OFF");
    Serial.printf("NORM VIEW : %s\n", showNormalized ? "ON" : "OFF");
#if ENABLE_FIREBASE
    Serial.printf("FIREBASE  : %s (ready=%d, wifi=%d)\n", firebaseEnabled ? "ON" : "OFF", firebaseReady,
                  (int)WiFi.status());
#endif
    Serial.println();
    Serial.println("=== CALIBRATION ===");
    for (int i = 0; i < CH_COUNT; i++) {
        LdrChannel &c = channels[i];
        Serial.printf("%-4s min=%-6d max=%-6d span=%-6d norm=%d\n", c.name, c.calMin, c.calMax,
                      c.calMax - c.calMin, normalized(i));
    }
}

void resetStats() {
    for (int i = 0; i < CH_COUNT; i++) {
        LdrChannel &c = channels[i];
        c.minRaw = ADC_FULL_SCALE;
        c.maxRaw = 0;
        c.stuckLow = 0;
        c.stuckHigh = 0;
        c.lastChangeMs = millis();
    }
    Serial.println("[STAT] Da reset thong ke runtime.");
}

void clearCalibration() {
    for (int i = 0; i < CH_COUNT; i++) {
        channels[i].calMin = -1;
        channels[i].calMax = -1;
    }
    Serial.println("[CAL] Da xoa ket qua calibration.");
}

void handleCommands() {
    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c < 0) break;
        switch (c) {
            case 'h':
            case 'H':
                printHelp();
                break;
            case 'c':
            case 'C':
                startCalibration();
                break;
            case 'r':
                resetStats();
                break;
            case 'R':
                clearCalibration();
                break;
            case 'l':
            case 'L':
                csvEnabled = !csvEnabled;
                if (csvEnabled) {
                    Serial.println("CSV HEADER: ms,TL,TR,BL,BR,diff,total,normTL,normTR,normBL,normBR,doTL,doTR,doBL,doBR");
                }
                Serial.printf("[CSV] %s\n", csvEnabled ? "ON" : "OFF");
                break;
            case 'x':
            case 'X':
                showNormalized = !showNormalized;
                Serial.printf("[VIEW] normalized %s\n", showNormalized ? "ON" : "OFF");
                break;
            case 'p':
            case 'P':
                printConfigAndCalib();
                break;
            case 'e':
            case 'E':
                exportConstants();
                break;
#if ENABLE_FIREBASE
            case 'f':
            case 'F':
                firebaseEnabled = !firebaseEnabled;
                Serial.printf("[FIREBASE] %s\n", firebaseEnabled ? "ON" : "OFF");
                break;
#endif
            default:
                break;
        }
    }
}

// ==============================================================================
// 10. SETUP / LOOP
// ==============================================================================

void setup() {
    Serial.begin(115200);
    delay(1200); // cho Serial CDC on-board ổn định
    printBanner();

    analogReadResolution(12);
    for (int i = 0; i < CH_COUNT; i++) {
        LdrChannel &c = channels[i];
        analogSetPinAttenuation(c.pin, ADC_11db);
        pinMode(c.pin, INPUT);
        c.lastChangeMs = millis();
#if USE_DO_OUTPUTS
        if (c.doPin >= 0) {
            pinMode(c.doPin, INPUT_PULLUP);
            c.doRaw = digitalRead(c.doPin);
            c.doBright = DO_ACTIVE_LOW ? (!c.doRaw) : c.doRaw;
        }
#endif
    }
    Serial.println("[ADC] 12-bit, ADC1 pins GPIO4..7, attenuation 11dB.");

    readAllSensors();
    printTable();
    printHelp();

#if ENABLE_FIREBASE
    connectWifi();
    initFirebase();
#endif
}

void loop() {
    readAllSensors();
    handleCommands();

    unsigned long now = millis();
    if (now - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
        lastDisplayMs = now;
        printTable();
    }
    if (csvEnabled && now - lastCsvMs >= CSV_INTERVAL_MS) {
        lastCsvMs = now;
        printCsv();
    }
    if (now - lastSelfTestMs >= SELFTEST_INTERVAL_MS) {
        lastSelfTestMs = now;
        printSelfTest();
    }
    updateCalibration(now);
#if ENABLE_FIREBASE
    if (now - lastFirebaseMs >= FIREBASE_INTERVAL_MS) {
        lastFirebaseMs = now;
        sendFirebase();
    }
#endif
    delay(5);
}
