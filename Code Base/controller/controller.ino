/* =============================================================================
 *  SolarController - MAIN FIRMWARE  (v1.0.0)
 *  -----------------------------------------------------------------------------
 *  Board : ESP32-S3 DevKit (N16R8) - Arduino IDE
 *  Flow  : Flow Chart/FlowChart.drawio
 *
 *  Start -> Init -> WiFi/Firebase? --NO--> Offline mode --+
 *                    | YES                                 |
 *              Dashboard command                           |
 *                    |                                     |
 *              Read sensors (LDR + Battery V/SOC) <--------+
 *                    |
 *              Send status (Realtime Telemetry)
 *                    |
 *              Auto mode? --NO--> Manual adjust -----------+
 *                    | YES                                 |
 *              Fault / low battery? --YES--> Stop + Alarm -+
 *                    | NO                                  |
 *              Night (dark)? --YES--> Park East -> Sleep --+
 *                    | NO                                  |
 *              |diff| >= deadband? --NO--> Hold -----------+
 *                    | YES                                 |
 *              Rotate toward sun --------------------------+
 *                    |                                     |
 *              Monitor battery/charging -> Update dashboard -> loop
 *
 *  Đấu chân:
 *      LDR AO : TL=GPIO4  TR=GPIO5  BL=GPIO6  BR=GPIO7   (ADC1)
 *      LDR DO : TL=GPIO15 TR=GPIO16 BL=GPIO17 BR=n/a      (DO_BR bỏ, servo giữ 18)
 *      Servo  : GPIO18 (signal), VCC 5V NGUỒN NGOÀI, chung GND
 *      VBAT   : GPIO8 (ADC1_CH7) - voltage divider (tỉ lệ VBAT_DIVIDER)
 *      Buzzer : GPIO11 (active), đặt PIN_BUZZER=-1 nếu không gắn
 *
 *  Serial (115200): h=help  a=Auto  m=Manual  +/-=gi angle  p=print state
 * ============================================================================= */

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ESP32Servo.h>

#define ENABLE_FIREBASE 1
#if ENABLE_FIREBASE
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>
#endif

// ==============================================================================
// 1. CẤU HÌNH
// ==============================================================================

#define FW_VERSION "1.1.0"

// --- Pin ---
#define PIN_LDR_TL 4
#define PIN_LDR_TR 5
#define PIN_LDR_BL 6
#define PIN_LDR_BR 7

#define USE_DO_OUTPUTS 1
#define PIN_DO_TL 15
#define PIN_DO_TR 16
#define PIN_DO_BL 17
#define PIN_DO_BR -1 // bỏ DO BR - servo dùng GPIO18
#define DO_ACTIVE_LOW 1

#define PIN_SERVO 18
#define SERVO_MIN_US 500
#define SERVO_MAX_US 2400

#define PIN_VBAT 8 // ADC1_CH7
#define PIN_BUZZER -1 // sơ đồ không có buzzer - alarm chỉ qua Dashboard flag

// 18650 Li-ion 3.7V (TP4056 sạc). Divider 100k/100k từ BAT+ -> GPIO8 (tỉ lệ 2.0)
#define VBAT_DIVIDER 2.0f
#define VBAT_CAL 1.0f // hệ số hiệu chỉnh đo thực tế

// --- Ngưỡng (Li-ion 1 cell) ---
#define NIGHT_THRESHOLD 250 // totalAvg < -> NIGHT
#define DEADBAND_THRESHOLD 60
#define LOW_BATTERY_V 3.2f
#define SOC_MIN_V 3.0f
#define SOC_MAX_V 4.2f
#define CHARGE_FULL_V 4.15f

// --- Servo ---
#define SERVO_EAST_ANGLE 0 // đỗ ban đêm
#define SERVO_WEST_ANGLE 180
#define SERVO_CENTER 90
#define TRACK_STEP 5 // độ/bước quay
// Hướng quay: +1 = side trái sáng -> tăng angle (hướng Tây). Đảo nếu lắp ngược.
#define TRACK_DIR 1

// --- Chu kỳ (ms) ---
#define DISPLAY_INTERVAL_MS 500
#define FIREBASE_WRITE_MIN_MS 1000
#define CONFIG_READ_INTERVAL_MS 1000
#define WIFI_RETRY_INTERVAL_MS 15000
#define WIFI_CONNECT_TIMEOUT_MS 12000
#define NIGHT_SLEEP_MS 10000 // light-sleep standby ban đêm
#define FAULT_COUNT_LIMIT 200

// --- Wi-Fi / Firebase ---
#define WIFI_SSID "hoangmanh"
#define WIFI_PASSWORD "00000000"
#if ENABLE_FIREBASE
#define FIREBASE_HOST                                                          \
  "https://solarcontroller-1cc93-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "XURINmjqSrUgYAs1B7TjQbZoe8Nn151O8BWGRKYs"
#define FB_TELEMETRY_PATH "/solar_controller/telemetry"
#define FB_CONFIG_PATH "/solar_controller/config"
#define FB_COMMAND_PATH "/solar_controller/command"
#endif

// ==============================================================================
// 2. BIẾN
// ==============================================================================

enum ChIdx { CH_TL = 0, CH_TR = 1, CH_BL = 2, CH_BR = 3, CH_COUNT = 4 };

struct LdrChannel {
    const char *name;
    uint8_t pin;
    int8_t doPin;
    int raw;
    int mv;
    bool doRaw;
    bool doBright;
    int stuckLow;
    int stuckHigh;
    int calMin;
    int calMax;
};

LdrChannel channels[CH_COUNT] = {
    {"TL", PIN_LDR_TL, PIN_DO_TL, 0, 0, false, false, 0, 0, -1, -1},
    {"TR", PIN_LDR_TR, PIN_DO_TR, 0, 0, false, false, 0, 0, -1, -1},
    {"BL", PIN_LDR_BL, PIN_DO_BL, 0, 0, false, false, 0, 0, -1, -1},
    {"BR", PIN_LDR_BR, PIN_DO_BR, 0, 0, false, false, 0, 0, -1, -1},
};

Servo panelServo;
bool servoAttached = false;
int currentAngle = SERVO_CENTER;

enum Mode { MODE_AUTO, MODE_MANUAL };
Mode runMode = MODE_AUTO;
int manualAngle = SERVO_CENTER;

float vbat = 0.0f;
float soc = 0.0f;
float vbatPrev = 0.0f;
int chargeState = 0; // -1 discharge, 0 idle, +1 charge, 2 full

bool sensorFault = false;
bool lowBattery = false;
bool offlineMode = true;
bool parkSleepPending = false;

Preferences prefs;
String wifiSsid = WIFI_SSID;
String wifiPass = WIFI_PASSWORD;
String serialBuf = "";

const char *lastAction = "INIT";
unsigned long lastDisplayMs = 0;
unsigned long lastFbWriteMs = 0;
unsigned long lastConfigReadMs = 0;
unsigned long lastWifiRetryMs = 0;
unsigned long lastVbatSampleMs = 0;
float vbatOlder = 0.0f;

#if ENABLE_FIREBASE
FirebaseData fbdo;
FirebaseConfig fbConfig;
FirebaseAuth auth;
bool firebaseReady = false;
String fbMode = "AUTO";
int fbManualAngle = SERVO_CENTER;
#endif

// ==============================================================================
// 3. TÍNH TOÁN
// ==============================================================================

int leftAvg() { return (channels[CH_TL].raw + channels[CH_BL].raw) / 2; }
int rightAvg() { return (channels[CH_TR].raw + channels[CH_BR].raw) / 2; }
int totalAvg() { return (leftAvg() + rightAvg()) / 2; }
int diffVal() { return leftAvg() - rightAvg(); }

bool isNight() { return totalAvg() < NIGHT_THRESHOLD; }

bool sensorFaultNow() {
    for (int i = 0; i < CH_COUNT; i++) {
        if (channels[i].stuckLow > FAULT_COUNT_LIMIT) return true;
        if (channels[i].stuckHigh > FAULT_COUNT_LIMIT) return true;
    }
    return false;
}

float readVbat() {
    long sum = 0;
    for (int s = 0; s < 16; s++) {
        sum += analogRead(PIN_VBAT);
        delayMicroseconds(200);
    }
    float vadc = (sum / 16.0f) * 3.3f / 4095.0f;
    return vadc * VBAT_DIVIDER * VBAT_CAL;
}

float calcSoc(float v) {
    float s = (v - SOC_MIN_V) * 100.0f / (SOC_MAX_V - SOC_MIN_V);
    return constrain(s, 0.0f, 100.0f);
}

const char *dayNightState() { return isNight() ? "NIGHT" : "DAY"; }

const char *modeStr() { return runMode == MODE_AUTO ? "AUTO" : "MANUAL"; }

const char *chargeStr() {
    switch (chargeState) {
        case 1: return "CHARGING";
        case 2: return "FULL";
        case -1: return "DISCHARGING";
        default: return "IDLE";
    }
}

// ==============================================================================
// 4. ĐỌC CẢM BIẾN  (flowchart: Read and process sensor data)
// ==============================================================================

void readSensors() {
    for (int i = 0; i < CH_COUNT; i++) {
        LdrChannel &c = channels[i];
        long sum = 0;
        for (int s = 0; s < 16; s++) {
            sum += analogRead(c.pin);
            delayMicroseconds(200);
        }
        c.raw = (int)(sum / 16);
        c.mv = (int)((long)c.raw * 3300 / 4095);

        if (c.raw <= 1) c.stuckLow++; else c.stuckLow = 0;
        if (c.raw >= 4094) c.stuckHigh++; else c.stuckHigh = 0;

#if USE_DO_OUTPUTS
        if (c.doPin >= 0) {
            c.doRaw = digitalRead(c.doPin);
            c.doBright = DO_ACTIVE_LOW ? !c.doRaw : c.doRaw;
        }
#endif
    }

    vbatPrev = vbat;
    vbat = readVbat();
    soc = calcSoc(vbat);
    sensorFault = sensorFaultNow();
    lowBattery = vbat > 0.5f && vbat < LOW_BATTERY_V;
}

// flowchart: Monitor Battery Voltage & Charging State
void monitorBattery() {
    unsigned long now = millis();
    if (lastVbatSampleMs == 0) {
        lastVbatSampleMs = now;
        vbatOlder = vbat;
        return;
    }
    if (now - lastVbatSampleMs >= 15000) {
        lastVbatSampleMs = now;
        float dv = vbat - vbatOlder;
        vbatOlder = vbat;
        if (vbat >= CHARGE_FULL_V) chargeState = 2;
        else if (dv > 0.03f) chargeState = 1;
        else if (dv < -0.03f) chargeState = -1;
        else chargeState = 0;
    }
}

// ==============================================================================
// 5. SERVO  (Manual adjust / Park East / Rotate / Hold / Stop)
// ==============================================================================

void servoEnsureAttach() {
    if (!servoAttached) {
        ESP32PWM::allocateTimer(0);
        panelServo.setPeriodHertz(50);
        panelServo.attach(PIN_SERVO, SERVO_MIN_US, SERVO_MAX_US);
        panelServo.write(currentAngle);
        servoAttached = true;
    }
}

void servoWriteAngle(int angle) {
    currentAngle = constrain(angle, SERVO_EAST_ANGLE, SERVO_WEST_ANGLE);
    servoEnsureAttach();
    panelServo.write(currentAngle);
}

void servoStopSafe() { // flowchart: Stop motor completely & switch to safe mode
    if (servoAttached) {
        panelServo.detach();
        servoAttached = false;
    }
}

void actionManual() {
    servoWriteAngle(manualAngle);
    lastAction = "MANUAL";
}

void actionStopAlarm() {
    servoStopSafe();
    lastAction = "SAFE_STOP";
}

void actionParkEast() {
    servoWriteAngle(SERVO_EAST_ANGLE);
    lastAction = "PARK_EAST";
    parkSleepPending = true;
}

void actionHold() {
    lastAction = "HOLD"; // motor remains idle
}

void actionRotate() { // flowchart: Rotate motor towards brighter side
    int step = TRACK_STEP * TRACK_DIR;
    if (diffVal() < 0) step = -step; // phải sáng hơn -> giảm angle (hướng Đông)
    servoWriteAngle(currentAngle + step);
    lastAction = diffVal() > 0 ? "TRACK_L" : "TRACK_R";
}

// ==============================================================================
// 6. ALARM
// ==============================================================================

void alarmUpdate() {
    if (PIN_BUZZER < 0) return;
    if (sensorFault || lowBattery) {
        digitalWrite(PIN_BUZZER, (millis() / 200) % 2 == 0 ? HIGH : LOW);
    } else {
        digitalWrite(PIN_BUZZER, LOW);
    }
}

// ==============================================================================
// 7. WI-FI / FIREBASE
// ==============================================================================

void wifiLoadPrefs() {
    prefs.begin("solarcfg", false);
    wifiSsid = prefs.getString("wifi_ssid", WIFI_SSID);
    wifiPass = prefs.getString("wifi_pass", WIFI_PASSWORD);
    prefs.end();
    if (wifiSsid.length() == 0) wifiSsid = WIFI_SSID;
    Serial.printf("[WIFI] NVS ssid=%s\n", wifiSsid.c_str());
}

bool wifiSavePrefs(const String &ssid, const String &pass) {
    prefs.begin("solarcfg", false);
    bool ok = prefs.putString("wifi_ssid", ssid) > 0;
    ok = (prefs.putString("wifi_pass", pass) >= 0) && ok;
    prefs.end();
    return ok;
}

bool wifiConnect(const String &ssid, const String &pass, unsigned long timeoutMs) {
    Serial.printf("[WIFI] ket noi %s ", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
        delay(200);
        Serial.print('.');
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] OK IP=%s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println("[WIFI] FAIL");
    return false;
}

// Doi WiFi tu Serial hoac Cloud. Luu NVS; sai mat khau -> khoi phuc WiFi cu.
bool wifiApply(const String &ssidIn, const String &passIn) {
    String ssid = ssidIn;
    ssid.trim();
    if (ssid.length() == 0) {
        Serial.println("[WIFI] APPLY FAIL ssid rong");
        return false;
    }
    if (ssid == wifiSsid && passIn == wifiPass && WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] APPLY OK ip=%s (khong doi)\n", WiFi.localIP().toString().c_str());
        offlineMode = false;
        return true;
    }

    String oldSsid = wifiSsid;
    String oldPass = wifiPass;
    wifiSsid = ssid;
    wifiPass = passIn;
    wifiSavePrefs(wifiSsid, wifiPass);

    WiFi.disconnect(true);
    delay(200);
    bool ok = wifiConnect(wifiSsid, wifiPass, 20000);
    if (!ok) {
        Serial.println("[WIFI] khoi phuc WiFi cu");
        wifiSsid = oldSsid;
        wifiPass = oldPass;
        wifiSavePrefs(wifiSsid, wifiPass);
        wifiConnect(wifiSsid, wifiPass, 15000);
        offlineMode = WiFi.status() != WL_CONNECTED;
        Serial.println("[WIFI] APPLY FAIL");
        return false;
    }
    offlineMode = false;
    Serial.printf("[WIFI] APPLY OK ip=%s\n", WiFi.localIP().toString().c_str());
    return true;
}

#if ENABLE_FIREBASE

void wifiBegin() {
    wifiConnect(wifiSsid, wifiPass, WIFI_CONNECT_TIMEOUT_MS);
    if (WiFi.status() != WL_CONNECTED) Serial.println("[WIFI] offline mode");
}

void firebaseInit() {
    fbConfig.host = FIREBASE_HOST;
    fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&fbConfig, &auth);
    Firebase.reconnectWiFi(true);
    firebaseReady = true;
    Serial.println("[FB] khoi tao xong");
}

void connectivityCheck() { // flowchart: WiFi & Firebase Connected & Available?
    bool wifiOk = WiFi.status() == WL_CONNECTED;
    if (wifiOk && firebaseReady && Firebase.ready()) {
        if (offlineMode) Serial.println("[NET] online");
        offlineMode = false;
        return;
    }
    if (offlineMode) {
        unsigned long now = millis();
        if (now - lastWifiRetryMs >= WIFI_RETRY_INTERVAL_MS) {
            lastWifiRetryMs = now;
            wifiBegin();
        }
    } else {
        Serial.println("[NET] mat ket noi -> offline mode");
        offlineMode = true;
    }
}

void applyConfigDoc(JsonDocument &doc, const char *path) { // Dashboard command
    if (doc["mode"].is<const char *>()) {
        String m = doc["mode"].as<String>();
        m.toUpperCase();
        runMode = (m == "MANUAL") ? MODE_MANUAL : MODE_AUTO;
    }
    if (doc["manual_angle"].is<int>()) {
        manualAngle = constrain(doc["manual_angle"].as<int>(), SERVO_EAST_ANGLE, SERVO_WEST_ANGLE);
    }
    if (doc["angle"].is<int>()) { // Dashboard gui command {mode, angle}
        manualAngle = constrain(doc["angle"].as<int>(), SERVO_EAST_ANGLE, SERVO_WEST_ANGLE);
    }

    String newSsid = doc["wifi_ssid"] | "";
    String newPass = doc["wifi_pass"] | "";
    newSsid.trim();
    if (newSsid.length() > 0) {
        Serial.printf("[NET] nhan WiFi tu cloud ssid=%s\n", newSsid.c_str());
        wifiApply(newSsid, newPass);
        String base = String(path);
        Firebase.RTDB.setString(&fbdo, base + "/wifi_ssid", "");
        Firebase.RTDB.setString(&fbdo, base + "/wifi_pass", "");
    }
}

void readCloudConfig(const char *path) {
    if (!Firebase.RTDB.getJSON(&fbdo, path)) return;
    JsonDocument doc;
    if (deserializeJson(doc, fbdo.payload()) != DeserializationError::Ok) return;
    applyConfigDoc(doc, path);
}

void dashboardCommand() { // flowchart: Dashboard command
    if (offlineMode) return;
    unsigned long now = millis();
    if (now - lastConfigReadMs < CONFIG_READ_INTERVAL_MS) return;
    lastConfigReadMs = now;

    readCloudConfig(FB_CONFIG_PATH);
    readCloudConfig(FB_COMMAND_PATH);
}

void sendTelemetry(const char *stage) {
    if (offlineMode) return;
    unsigned long now = millis();
    if (now - lastFbWriteMs < FIREBASE_WRITE_MIN_MS) return;
    lastFbWriteMs = now;

    FirebaseJson json;
    json.set("ldr_tl", channels[CH_TL].raw);
    json.set("ldr_tr", channels[CH_TR].raw);
    json.set("ldr_bl", channels[CH_BL].raw);
    json.set("ldr_br", channels[CH_BR].raw);
    json.set("diff", diffVal());
    json.set("total", totalAvg());
    json.set("light_state", dayNightState());
    json.set("mode", modeStr());
    json.set("action", lastAction);
    json.set("angle", currentAngle);
    json.set("servo_on", servoAttached ? 1 : 0);
    json.set("vbat", (double)((int)(vbat * 100)) / 100.0);
    json.set("voltage", (double)((int)(vbat * 100)) / 100.0); // alias cho dashboard
    json.set("soc", (double)((int)(soc * 10)) / 10.0);
    json.set("charge", chargeStr());
    json.set("fault", sensorFault ? 1 : 0);
    json.set("low_batt", lowBattery ? 1 : 0);
    json.set("offline", offlineMode ? 1 : 0);
    json.set("stage", stage);
    json.set("rssi", WiFi.RSSI());
    json.set("wifi_ssid", wifiSsid);
    json.set("wifi_ip", WiFi.localIP().toString());
    json.set("uptime_s", (uint32_t)(millis() / 1000));

    if (!Firebase.RTDB.setJSON(&fbdo, FB_TELEMETRY_PATH, &json)) {
        Serial.printf("[FB] WRITE FAIL code=%d\n", fbdo.errorCode());
    }
}

#else
void connectivityCheck() { offlineMode = true; }
void dashboardCommand() {}
void sendTelemetry(const char *) {}
#endif

// ==============================================================================
// 8. SLEEP  (flowchart: Enter low-power sleep standby mode)
// ==============================================================================

void nightSleep() {
    Serial.printf("[SLEEP] light sleep %d ms\n", NIGHT_SLEEP_MS);
    Serial.flush();
    esp_sleep_enable_timer_wakeup((uint64_t)NIGHT_SLEEP_MS * 1000ULL);
    esp_light_sleep_start();
    WiFi.RSSI(); // touch để modem resume
}

// ==============================================================================
// 9. SERIAL COMMAND
// ==============================================================================

void printState() {
    Serial.println();
    Serial.printf("MODE=%s ANGLE=%d ACTION=%s STATE=%s\n", modeStr(), currentAngle, lastAction,
                  dayNightState());
    Serial.printf("LDR TL=%4d TR=%4d BL=%4d BR=%4d | DIFF=%+d TOTAL=%d\n", channels[CH_TL].raw,
                  channels[CH_TR].raw, channels[CH_BL].raw, channels[CH_BR].raw, diffVal(), totalAvg());
    Serial.printf("VBAT=%.2fV SOC=%.0f%% CHARGE=%s FAULT=%d LOWBATT=%d NET=%s\n", vbat, soc, chargeStr(),
                  sensorFault, lowBattery, offlineMode ? "OFFLINE" : "ONLINE");
}

void printHelp() {
    Serial.println();
    Serial.println("--- LENH SERIAL (ket thuc bang Enter) ---");
    Serial.println(" h              : tro giup");
    Serial.println(" a              : che do AUTO");
    Serial.println(" m              : che do MANUAL");
    Serial.println(" angle <0..180> : dat goc MANUAL");
    Serial.println(" + / -          : dieu chinh goc MANUAL 5 do");
    Serial.println(" p              : in trang thai hien tai");
    Serial.println(" wifi|<ssid>|<pass> : doi WiFi, luu NVS");
    Serial.println("-----------------------------------------");
}

void processSerialCommand(String line) {
    line.trim();
    if (line.length() == 0) return;
    String lower = line;
    lower.toLowerCase();

    if (lower == "h" || lower == "help") {
        printHelp();
    } else if (lower == "a" || lower == "auto") {
        runMode = MODE_AUTO;
        Serial.println("[MODE] AUTO");
    } else if (lower == "m" || lower == "manual") {
        runMode = MODE_MANUAL;
        Serial.println("[MODE] MANUAL");
    } else if (lower == "+") {
        runMode = MODE_MANUAL;
        servoWriteAngle(currentAngle + 5);
        Serial.printf("[ANGLE] %d\n", currentAngle);
    } else if (lower == "-") {
        runMode = MODE_MANUAL;
        servoWriteAngle(currentAngle - 5);
        Serial.printf("[ANGLE] %d\n", currentAngle);
    } else if (lower == "p" || lower == "status") {
        printState();
    } else if (lower.startsWith("angle ")) {
        int a = lower.substring(6).toInt();
        runMode = MODE_MANUAL;
        manualAngle = constrain(a, SERVO_EAST_ANGLE, SERVO_WEST_ANGLE);
        servoWriteAngle(manualAngle);
        Serial.printf("[ANGLE] %d\n", currentAngle);
    } else if (lower.startsWith("wifi|")) {
        int p1 = line.indexOf('|');
        int p2 = line.indexOf('|', p1 + 1);
        if (p1 < 0 || p2 < 0) {
            Serial.println("[WIFI] APPLY FAIL format wifi|<ssid>|<pass>");
            return;
        }
        String ssid = line.substring(p1 + 1, p2);
        String pass = line.substring(p2 + 1);
        ssid.trim();
        Serial.printf("[WIFI] nhan lenh Serial ssid=%s\n", ssid.c_str());
        wifiApply(ssid, pass);
    } else if (lower == "wifi") {
        Serial.println("[WIFI] dung: wifi|<ssid>|<pass>");
    } else {
        Serial.printf("[CMD] khong hieu: %s\n", line.c_str());
    }
}

void handleSerial() {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            processSerialCommand(serialBuf);
            serialBuf = "";
        } else if (serialBuf.length() < 192) {
            serialBuf += c;
        }
    }
}

// ==============================================================================
// 10. SETUP / LOOP
// ==============================================================================

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println();
    Serial.println("=========================================================");
    Serial.println("   SolarController MAIN  v" FW_VERSION);
    Serial.println("   ESP32-S3 | 4x LDR + Servo + 18650 VBAT divider + Firebase");
    Serial.println("=========================================================");
    Serial.printf("[PIN] LDR AO 4/5/6/7  DO 15/16/17/-1  SERO=18  VBAT=8  BUZ=%d\n", PIN_BUZZER);

    analogReadResolution(12);
    for (int i = 0; i < CH_COUNT; i++) {
        LdrChannel &c = channels[i];
        analogSetPinAttenuation(c.pin, ADC_11db);
        pinMode(c.pin, INPUT);
#if USE_DO_OUTPUTS
        if (c.doPin >= 0) pinMode(c.doPin, INPUT_PULLUP);
#endif
    }
    analogSetPinAttenuation(PIN_VBAT, ADC_11db);
    if (PIN_BUZZER >= 0) {
        pinMode(PIN_BUZZER, OUTPUT);
        digitalWrite(PIN_BUZZER, LOW);
    }

    servoEnsureAttach();
    panelServo.write(SERVO_CENTER);
    currentAngle = SERVO_CENTER;

    wifiLoadPrefs();
#if ENABLE_FIREBASE
    wifiBegin();
    if (WiFi.status() == WL_CONNECTED) firebaseInit();
    offlineMode = WiFi.status() != WL_CONNECTED;
#else
    offlineMode = true;
#endif

    readSensors();
    monitorBattery();
    printState();
    printHelp();
}

void loop() {
    connectivityCheck();            // 1. WiFi & Firebase OK?
    dashboardCommand();             // 2. Dashboard command (mode, angle)
    readSensors();                  // 3. Read LDR + battery
    sendTelemetry("read");          // 4. Send status (realtime telemetry)
    parkSleepPending = false;

    if (runMode == MODE_MANUAL) {   // 5. Auto mode?
        actionManual();             //    NO -> manual adjust
    } else if (sensorFault || lowBattery) { // 6. Fault / low battery?
        actionStopAlarm();          //    YES -> stop motor + safe mode
        // alarm sounds + alert via telemetry
    } else if (isNight()) {         // 7. Night?
        actionParkEast();           //    YES -> park East -> sleep
    } else if (abs(diffVal()) >= DEADBAND_THRESHOLD) { // 8. Deadband?
        actionRotate();             //    YES -> rotate toward sun
    } else {
        actionHold();               //    NO -> hold angle
    }

    monitorBattery();               // 9. Monitor battery & charging
    sendTelemetry("update");        // 10. Update dashboard -> loop
    alarmUpdate();

    if (parkSleepPending) nightSleep();

    if (millis() - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
        lastDisplayMs = millis();
        printState();
    }
    handleSerial();
    delay(5);
}
