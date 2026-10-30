/* =============================================================================
 *  SolarController - MAIN FIRMWARE  (v2.0.0)
 *  -----------------------------------------------------------------------------
 *  Board : ESP32-S3 DevKit (N16R8) - Arduino IDE
 *  Servo : SG90 cắt trục → position control 0°-360°
 *  Flow  : Flow Chart/FlowChart.drawio
 *
 *  Start -> Init -> WiFi/Firebase? --NO--> Offline mode --+
 *                    | YES                                 |
 *              Dashboard command                           |
 *                    |                                     |
 *              Read sensors (LDR + Battery V/SOC) <--------+
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
 *              Monitor battery/charging -> Send Telemetry -> loop
 *
 *  La bàn góc quay (nhìn từ trên, chiều kim đồng hồ):
 *      0° = Bắc  |  90° = Đông  |  180° = Nam  |  270° = Tây
 *  Park ban đêm = 90° (hướng Đông, đón bình minh)
 *  Quỹ đạo mặt trời: Đông (90°) → Nam (180°) → Tây (270°)
 *
 *  Đấu chân:
 *      LDR AO : TL=GPIO4  TR=GPIO5  BL=GPIO6  BR=GPIO7   (ADC1)
 *      LDR DO : TL=GPIO15 TR=GPIO16 BL=GPIO17 BR=n/a      (DO_BR bỏ, servo giữ 18)
 *      Servo  : GPIO18 (signal), VCC 5V NGUỒN NGOÀI, chung GND
 *      VBAT   : GPIO8 (ADC1_CH7) - voltage divider (tỉ lệ VBAT_DIVIDER)
 *      Buzzer : GPIO11 (active), đặt PIN_BUZZER=-1 nếu không gắn
 *
 *  Serial (115200): h=help  a=Auto  m=Manual  +/-=dieu chinh angle  p=print state
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

#define FW_VERSION "2.0.0"

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

// --- Servo (SG90 cắt trục = CONTINUOUS rotation, điều khiển bằng THỜI GIAN) ---
// Không có feedback vị trí: xung chỉ quyết định tốc độ/chiều.
//   ~1500us = dừng | 500us = quay CCW hết tốc | 2400us = quay CW hết tốc
#define SERVO_STOP_US 1500
#define SERVO_CW_US 2400
#define SERVO_CCW_US 500
#define SERVO_SETTLE_MS 250 // nghỉ cơ khí giữa home -> đích
#define TRACK_PULSE_MS 400  // mỗi nhịp auto tracking chạy bao lâu
#define TRACK_PAUSE_MS 800  // nghỉ giữa 2 nhịp tracking
// AUTO tracking: left sáng hơn -> quay CW (tăng hướng Đông->Tây)
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
#define FB_MOTION_PATH "/solar_controller/config/motion"
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

// --- Cấu hình chuyển động (nhập từ web, lưu NVS) ---
struct MotionCfg {
    int stopUs;
    int cwUs;
    int ccwUs;
    int homeMs;
    int tlMs;
    int trMs;
    int blMs;
    int brMs;
    int tlDir; // +1 = CW, -1 = CCW
    int trDir;
    int blDir;
    int brDir;
};
MotionCfg motion = {SERVO_STOP_US, SERVO_CW_US, SERVO_CCW_US, 3000, 0, 1000, 3000, 2000,
                    1, 1, 1, 1};

enum MovePhase { MV_IDLE, MV_HOME, MV_SETTLE, MV_TARGET, MV_RUN };
MovePhase mvPhase = MV_IDLE;
unsigned long mvEndMs = 0;
unsigned long mvTargetMs = 0;
unsigned long mvStartMs = 0;
int mvDir = 1;
String mvTarget = "-";
bool moving = false;

// --- Kết quả lần chạy vừa dừng (dùng cho hiệu chỉnh thời gian trên web) ---
unsigned long lastMoveMs = 0;
String lastMoveTarget = "-";
uint32_t moveSeq = 0; // tăng mỗi lần 1 chuyển động kết thúc -> web biết có số mới

enum Mode { MODE_AUTO, MODE_MANUAL };
Mode runMode = MODE_MANUAL; // boot: đứng yên tại TL, chỉ chạy khi có lệnh
int manualAngle = 0;        // giữ để tương thích telemetry cũ (không điều khiển servo)

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
int fbManualAngle = 0;
uint64_t lastCmdTimestamp = 0; // tránh áp dụng command cũ (Date.now() ms > 2^32)
bool cmdApplied = false;
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
    // < 2.0 V = divider chưa nối / pin không tồn tại -> không báo low-batt giả
    lowBattery = vbat > 2.0f && vbat < LOW_BATTERY_V;
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
// 5. SERVO CONTINUOUS  (xung = tốc độ, thời gian = quãng đường)
// ==============================================================================

void servoEnsureAttach() {
    if (!servoAttached) {
        ESP32PWM::allocateTimer(0);
        panelServo.setPeriodHertz(50);
        panelServo.attach(PIN_SERVO, SERVO_MIN_US, SERVO_MAX_US);
        servoAttached = true;
    }
}

// Ghi trực tiếp xung (us) - KHÔNG dùng write() vì nó kẹp góc > 180
void servoPulse(int us) {
    servoEnsureAttach();
    panelServo.writeMicroseconds(constrain(us, SERVO_MIN_US, SERVO_MAX_US));
}

void servoStop() {
    servoPulse(motion.stopUs);
    moving = false;
}

void servoStopSafe() { // flowchart: Stop motor completely & switch to safe mode
    if (servoAttached) {
        panelServo.detach();
        servoAttached = false;
    }
    moving = false;
    mvPhase = MV_IDLE;
    mvTarget = "-";
}

void motionApplyDoc(JsonDocument &doc) { // /solar_controller/config/motion
    auto getInt = [&](const char *k, int lo, int hi, int &out) {
        if (doc[k].is<int>()) out = constrain(doc[k].as<int>(), lo, hi);
    };
    getInt("stop_us", 1000, 2000, motion.stopUs);
    getInt("cw_us", 1200, 2400, motion.cwUs);
    getInt("ccw_us", 500, 1800, motion.ccwUs);
    getInt("home_ms", 0, 20000, motion.homeMs);
    getInt("tl_ms", 0, 20000, motion.tlMs);
    getInt("tr_ms", 0, 20000, motion.trMs);
    getInt("bl_ms", 0, 20000, motion.blMs);
    getInt("br_ms", 0, 20000, motion.brMs);
    getInt("tl_dir", -1, 1, motion.tlDir);
    getInt("tr_dir", -1, 1, motion.trDir);
    getInt("bl_dir", -1, 1, motion.blDir);
    getInt("br_dir", -1, 1, motion.brDir);
}

void motionSavePrefs() {
    prefs.begin("motion", false);
    prefs.putInt("stop_us", motion.stopUs);
    prefs.putInt("cw_us", motion.cwUs);
    prefs.putInt("ccw_us", motion.ccwUs);
    prefs.putInt("home_ms", motion.homeMs);
    prefs.putInt("tl_ms", motion.tlMs);
    prefs.putInt("tr_ms", motion.trMs);
    prefs.putInt("bl_ms", motion.blMs);
    prefs.putInt("br_ms", motion.brMs);
    prefs.putInt("tl_dir", motion.tlDir);
    prefs.putInt("tr_dir", motion.trDir);
    prefs.putInt("bl_dir", motion.blDir);
    prefs.putInt("br_dir", motion.brDir);
    prefs.end();
}

void motionLoadPrefs() {
    prefs.begin("motion", true);
    motion.stopUs = prefs.getInt("stop_us", motion.stopUs);
    motion.cwUs = prefs.getInt("cw_us", motion.cwUs);
    motion.ccwUs = prefs.getInt("ccw_us", motion.ccwUs);
    motion.homeMs = prefs.getInt("home_ms", motion.homeMs);
    motion.tlMs = prefs.getInt("tl_ms", motion.tlMs);
    motion.trMs = prefs.getInt("tr_ms", motion.trMs);
    motion.blMs = prefs.getInt("bl_ms", motion.blMs);
    motion.brMs = prefs.getInt("br_ms", motion.brMs);
    motion.tlDir = prefs.getInt("tl_dir", motion.tlDir);
    motion.trDir = prefs.getInt("tr_dir", motion.trDir);
    motion.blDir = prefs.getInt("bl_dir", motion.blDir);
    motion.brDir = prefs.getInt("br_dir", motion.brDir);
    prefs.end();
}

// Chạy 1 nhịp: dir>0 = CW, dir<0 = CCW
void motionPulse(int dir, unsigned long ms, const char *label) {
    if (ms == 0) return;
    servoPulse(dir > 0 ? motion.cwUs : motion.ccwUs);
    moving = true;
    mvDir = dir;
    mvTargetMs = ms;
    mvTarget = label;
    mvPhase = MV_TARGET;
    mvStartMs = millis();
    mvEndMs = mvStartMs + ms;
    Serial.printf("[MV] pulse %s dir=%d ms=%lu\n", label, dir, ms);
}

// Chạy LIÊN TỤC (không tự hết giờ) để người dùng bấm GHI khi vừa đúng vị trí.
// An toàn: tự dừng sau MOTION_RUN_MAX_MS.
#define MOTION_RUN_MAX_MS 20000UL
void motionRun(int dir, const char *label) {
    servoPulse(dir > 0 ? motion.cwUs : motion.ccwUs);
    moving = true;
    mvDir = dir;
    mvTargetMs = 0;
    mvTarget = label;
    mvPhase = MV_RUN;
    mvStartMs = millis();
    mvEndMs = mvStartMs + MOTION_RUN_MAX_MS;
}

// Kết thúc chuyển động: ghi lại thời gian chạy thực tế + tăng moveSeq
void motionFinish() {
    if (mvStartMs > 0) {
        lastMoveMs = millis() - mvStartMs;
        lastMoveTarget = mvTarget;
        moveSeq++;
        Serial.printf("[MV] finish %s measured=%lums seq=%u\n", lastMoveTarget.c_str(), lastMoveMs,
                      (unsigned)moveSeq);
    }
    mvStartMs = 0;
    servoStop();
}

// Về home (CCW home_ms) rồi chạy tới hướng đích
void motionGoTarget(const String &target) {
    int ms = 0, dir = 1;
    String t = target;
    t.toUpperCase();
    if (t == "TL") { ms = motion.tlMs; dir = motion.tlDir; }
    else if (t == "TR") { ms = motion.trMs; dir = motion.trDir; }
    else if (t == "BL") { ms = motion.blMs; dir = motion.blDir; }
    else if (t == "BR") { ms = motion.brMs; dir = motion.brDir; }
    else if (t == "HOME") { ms = 0; }
    else return;

    mvTarget = t;
    mvTargetMs = ms;
    mvDir = dir;

    if (motion.homeMs > 0) {
        servoPulse(motion.ccwUs); // về mốc TL
        moving = true;
        mvPhase = MV_HOME;
        mvStartMs = millis();
        mvEndMs = mvStartMs + motion.homeMs;
        lastAction = "HOMING";
    } else if (ms > 0) {
        motionPulse(dir, ms, t.c_str());
        lastAction = "MOVE";
    } else {
        servoStop();
        lastAction = "STOP";
    }
}

// Chạy LIÊN TỤC tới 1 hướng (dùng để ĐO thời gian): TL/TR/BL/BR hoặc HOME
void motionRunTarget(const String &target) {
    String t = target;
    t.toUpperCase();
    if (t == "TL") motionRun(motion.tlDir, "TL");
    else if (t == "TR") motionRun(motion.trDir, "TR");
    else if (t == "BL") motionRun(motion.blDir, "BL");
    else if (t == "BR") motionRun(motion.brDir, "BR");
    else if (t == "HOME") motionRun(-1, "HOME");
    else return;
    lastAction = "MEASURE";
}

void motionStopNow() {
    if (moving || mvStartMs > 0) motionFinish(); // ghi last_ms + move_seq
    mvPhase = MV_IDLE;
    lastAction = "STOP";
}

// Máy trạng thái chuyển động - gọi mỗi loop, KHÔNG blocking
void motionTick() {
    if (mvPhase == MV_IDLE) return;
    unsigned long now = millis();
    if (now < mvEndMs) return;
    switch (mvPhase) {
        case MV_HOME:
            servoStop();
            mvPhase = MV_SETTLE;
            mvEndMs = now + SERVO_SETTLE_MS;
            break;
        case MV_SETTLE:
            if (mvTargetMs > 0) {
                motionPulse(mvDir, mvTargetMs, mvTarget.c_str());
                lastAction = "MOVE";
            } else {
                mvPhase = MV_IDLE;
                lastAction = "STOP";
            }
            break;
        case MV_TARGET:
            motionFinish();
            mvPhase = MV_IDLE;
            lastAction = "STOP";
            break;
        case MV_RUN: // hết thời gian an toàn
            motionFinish();
            mvPhase = MV_IDLE;
            lastAction = "STOP";
            break;
        default:
            mvPhase = MV_IDLE;
            break;
    }
}

void actionManual() {
    if (mvPhase == MV_IDLE) {
        servoStop(); // giữ xung dừng, không quay
        if (strcmp(lastAction, "MANUAL") != 0) lastAction = "MANUAL";
    }
}

void actionStopAlarm() {
    servoStopSafe();
    lastAction = "SAFE_STOP";
}

void actionParkEast() { // ban đêm: về mốc TL rồi ngủ
    motionGoTarget("TL");
    lastAction = "PARK_TL";
    parkSleepPending = true;
}

void actionHold() {
    if (mvPhase == MV_IDLE) {
        servoStop();
        lastAction = "HOLD";
    }
}

// AUTO: mỗi nhịp chạy TRACK_PULSE_MS rồi nghỉ TRACK_PAUSE_MS
unsigned long lastTrackMs = 0;
void actionRotate() {
    unsigned long now = millis();
    if (now - lastTrackMs < TRACK_PAUSE_MS) return;
    lastTrackMs = now;
    int diff = diffVal();
    int dir = (diff > 0) ? TRACK_DIR : -TRACK_DIR;
    motionPulse(dir, TRACK_PULSE_MS, dir > 0 ? "TRACK_CW" : "TRACK_CCW");
    lastAction = dir > 0 ? "TRACK_CW" : "TRACK_CCW";
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
    if (wifiOk && (!firebaseReady || !Firebase.ready())) {
        // WiFi OK nhưng Firebase rớt (TLS/timeout) -> khởi tạo lại định kỳ
        static unsigned long lastFbRetryMs = 0;
        unsigned long now = millis();
        if (now - lastFbRetryMs >= 10000) {
            lastFbRetryMs = now;
            Serial.println("[FB] ket noi lai...");
            firebaseInit();
            offlineMode = false;
            return;
        }
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
    // Kiểm tra timestamp để tránh áp dụng command cũ.
    // Date.now() ~1.7e12 > 32-bit -> ArduinoJson luu dang double, KHONG dung is<unsigned long>().
    double tsRaw = 0;
    if (doc["timestamp"].is<double>()) {
        tsRaw = doc["timestamp"].as<double>();
    } else if (doc["timestamp"].is<long long>()) {
        tsRaw = (double)doc["timestamp"].as<long long>();
    }
    if (tsRaw > 0) {
        uint64_t cmdTs = (uint64_t)tsRaw;
        if (cmdTs <= lastCmdTimestamp) return; // bỏ qua command cũ
        lastCmdTimestamp = cmdTs;
    }

    bool hasCommand = false;
    if (doc["mode"].is<const char *>()) {
        String m = doc["mode"].as<String>();
        m.toUpperCase();
        runMode = (m == "MANUAL") ? MODE_MANUAL : MODE_AUTO;
        hasCommand = true;
    }
    // Cấu hình chuyển động gửi kèm command (đường /command được phép ghi từ web)
    MotionCfg before = motion;
    motionApplyDoc(doc);
    if (memcmp(&before, &motion, sizeof(MotionCfg)) != 0) {
        motionSavePrefs();
        Serial.printf("[MOTION] cfg home=%d tl=%d tr=%d bl=%d br=%d cw=%d ccw=%d stop=%d\n",
                      motion.homeMs, motion.tlMs, motion.trMs, motion.blMs, motion.brMs, motion.cwUs,
                      motion.ccwUs, motion.stopUs);
        hasCommand = true;
    }
    // Lệnh chuyển động: {move:"TL"|"TR"|"BL"|"BR"|"HOME"} hoặc {dir:"CW"|"CCW", ms:...}
    if (doc["move"].is<const char *>()) {
        motionGoTarget(doc["move"].as<String>());
        hasCommand = true;
    }
    if (doc["dir"].is<const char *>()) {
        String d = doc["dir"].as<String>();
        d.toUpperCase();
        int ms = doc["ms"] | 0;
        if (ms <= 0) {
            motionRun(d == "CCW" ? -1 : 1, "MANUAL_RUN");
        } else {
            motionPulse(d == "CCW" ? -1 : 1, (unsigned long)constrain(ms, 0, 20000), "MANUAL_MOVE");
        }
        hasCommand = true;
    }
    // ĐO thời gian: {run:"TL"|"TR"|"BL"|"BR"|"HOME"} -> quay liên tục tới khi {stop:true}
    if (doc["run"].is<const char *>()) {
        motionRunTarget(doc["run"].as<String>());
        hasCommand = true;
    }
    if (doc["stop"].is<bool>() && doc["stop"].as<bool>()) {
        motionStopNow();
        hasCommand = true;
    }
    // Giữ tương thích: angle/manual_angle chỉ ghi nhận, KHÔNG điều khiển servo continuous
    if (doc["manual_angle"].is<int>()) {
        manualAngle = constrain(doc["manual_angle"].as<int>(), 0, 360);
        hasCommand = true;
    }
    if (doc["angle"].is<int>()) {
        manualAngle = constrain(doc["angle"].as<int>(), 0, 360);
        hasCommand = true;
    }

    String newSsid = doc["wifi_ssid"] | "";
    String newPass = doc["wifi_pass"] | "";
    newSsid.trim();
    if (newSsid.length() > 0) {
        Serial.printf("[NET] nhan WiFi tu cloud ssid=%s\n", newSsid.c_str());
        wifiApply(newSsid, newPass);
        hasCommand = true;
    }

    // Xóa command sau khi đã đọc để tránh áp dụng lặp lại
    if (hasCommand) {
        cmdApplied = true;
        Serial.printf("[CMD] apply mode=%s move=%s ts=%.0f path=%s\n", modeStr(), mvTarget.c_str(),
                      (double)lastCmdTimestamp, path);
        Firebase.RTDB.deleteNode(&fbdo, path);
    }
}

void readCloudConfig(const char *path) {
    if (!Firebase.RTDB.getJSON(&fbdo, path)) return;
    JsonDocument doc;
    if (deserializeJson(doc, fbdo.payload()) != DeserializationError::Ok) return;
    applyConfigDoc(doc, path);
}

// /solar_controller/config/motion : cấu hình thời gian chạy từ web, KHÔNG xóa
void readMotionConfig() {
    if (!Firebase.RTDB.getJSON(&fbdo, FB_MOTION_PATH)) return;
    JsonDocument doc;
    if (deserializeJson(doc, fbdo.payload()) != DeserializationError::Ok) return;
    MotionCfg before = motion;
    motionApplyDoc(doc);
    if (memcmp(&before, &motion, sizeof(MotionCfg)) != 0) {
        motionSavePrefs();
        Serial.printf("[MOTION] cfg home=%d tl=%d tr=%d bl=%d br=%d cw=%d ccw=%d stop=%d\n",
                      motion.homeMs, motion.tlMs, motion.trMs, motion.blMs, motion.brMs, motion.cwUs,
                      motion.ccwUs, motion.stopUs);
    }
}

void dashboardCommand() { // flowchart: Dashboard command
    if (offlineMode) return;
    unsigned long now = millis();
    // Khi đang chạy (home/đo/target) phải nghe lệnh DỪNG nhanh và tránh GET nặng
    // -> chỉ poll /command 150ms, không đọc config (tránh chặn loop làm sai thời gian)
    bool fast = (mvPhase != MV_IDLE);
    if (now - lastConfigReadMs < (fast ? 150UL : CONFIG_READ_INTERVAL_MS)) return;
    lastConfigReadMs = now;

    readCloudConfig(FB_COMMAND_PATH);
    if (fast) return; // đang chạy: chỉ chờ lệnh stop
    readCloudConfig(FB_CONFIG_PATH);
    // Không đọc /config/motion: rules Firebase chặn web path này;
    // cấu hình chuyển động nay đi kèm /command và được lưu vào NVS.
}

void sendTelemetry(const char *stage) {
    if (offlineMode) return;
    // Không ghi Firebase khi đang chạy: HTTPS write chặn loop ~200-300ms -> sai thời gian
    if (mvPhase != MV_IDLE) return;
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
    json.set("moving", moving ? 1 : 0);
    json.set("move_target", mvTarget);
    json.set("move_dir", mvDir > 0 ? "CW" : "CCW");
    json.set("move_elapsed_ms", (uint32_t)(moving && mvStartMs > 0 ? (millis() - mvStartMs) : 0));
    json.set("last_ms", (uint32_t)lastMoveMs);
    json.set("last_target", lastMoveTarget);
    json.set("move_seq", moveSeq);
    json.set("fw", "motion-v2");
    json.set("angle", manualAngle); // giữ field cũ cho dashboard, không phải góc thật
    json.set("manual_angle", manualAngle);
    json.set("home_ms", motion.homeMs);
    json.set("tl_ms", motion.tlMs);
    json.set("tr_ms", motion.trMs);
    json.set("bl_ms", motion.blMs);
    json.set("br_ms", motion.brMs);
    json.set("cw_us", motion.cwUs);
    json.set("ccw_us", motion.ccwUs);
    json.set("stop_us", motion.stopUs);
    json.set("tl_dir", motion.tlDir);
    json.set("tr_dir", motion.trDir);
    json.set("bl_dir", motion.blDir);
    json.set("br_dir", motion.brDir);
    json.set("cmd_ts", (double)lastCmdTimestamp); // ACK: timestamp lenh da ap dung
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
    // Khôi phục WiFi sau khi wake từ light sleep
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[SLEEP] WiFi mat sau sleep, dang ket noi lai...");
        WiFi.reconnect();
        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 5000) {
            delay(100);
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[SLEEP] WiFi OK IP=%s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("[SLEEP] WiFi FAIL - tiep tuc offline");
        }
    }
}

// ==============================================================================
// 9. SERIAL COMMAND
// ==============================================================================

void printState() {
    Serial.println();
    Serial.printf("MODE=%s ACTION=%s MOVING=%d TARGET=%s DIR=%s STATE=%s\n", modeStr(), lastAction,
                  moving ? 1 : 0, mvTarget.c_str(), mvDir > 0 ? "CW" : "CCW", dayNightState());
    Serial.printf("CFG home=%dms tl=%d tr=%d bl=%d br=%d | us cw=%d ccw=%d stop=%d\n", motion.homeMs,
                  motion.tlMs, motion.trMs, motion.blMs, motion.brMs, motion.cwUs, motion.ccwUs,
                  motion.stopUs);
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
    Serial.println(" tl/tr/bl/br    : ve home roi chay toi huong do");
    Serial.println(" home           : chi ve moc TL");
    Serial.println(" cw <ms>        : quay CW trong <ms> (0 = quay liên tục)");
    Serial.println(" ccw <ms>       : quay CCW trong <ms> (0 = quay liên tục)");
    Serial.println(" run <TL|TR|BL|BR|HOME> : quay liên tục để đo, dung bang 's'");
    Serial.println(" s              : dung servo ngay");
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
        motionStopNow();
        Serial.println("[MODE] MANUAL");
    } else if (lower == "s" || lower == "stop") {
        motionStopNow();
        Serial.println("[SERVO] STOP");
    } else if (lower == "tl" || lower == "tr" || lower == "bl" || lower == "br") {
        motionGoTarget(lower);
        Serial.printf("[SERVO] GO %s\n", lower.c_str());
    } else if (lower == "home") {
        motionGoTarget("HOME");
        Serial.println("[SERVO] HOME");
    } else if (lower.startsWith("cw ")) {
        int ms = constrain(lower.substring(3).toInt(), 0, 20000);
        if (ms == 0) motionRun(1, "MANUAL_RUN");
        else motionPulse(1, (unsigned long)ms, "MANUAL_CW");
        Serial.println("[SERVO] CW");
    } else if (lower.startsWith("ccw ")) {
        int ms = constrain(lower.substring(4).toInt(), 0, 20000);
        if (ms == 0) motionRun(-1, "MANUAL_RUN");
        else motionPulse(-1, (unsigned long)ms, "MANUAL_CCW");
        Serial.println("[SERVO] CCW");
    } else if (lower.startsWith("run ")) {
        motionRunTarget(lower.substring(4));
        Serial.printf("[SERVO] RUN %s\n", lower.substring(4).c_str());
    } else if (lower == "p" || lower == "status") {
        printState();
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
    motionLoadPrefs();
    servoStop(); // giữ xung dừng (1500us), không quay khi boot
    Serial.printf("[SERVO] attached=%d stop=%dus cw=%dus ccw=%dus home=%dms\n", servoAttached ? 1 : 0,
                  motion.stopUs, motion.cwUs, motion.ccwUs, motion.homeMs);

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
    motionTick();                   // 2. Tiến trình chuyển động (non-blocking)
    dashboardCommand();             // 3. Dashboard command (mode, move, config)
    motionTick();
    readSensors();                  // 4. Read LDR + battery
    motionTick();
    parkSleepPending = false;

    if (mvPhase != MV_IDLE) {
        // đang chạy lệnh home/target -> không cho auto ghi đè
    } else if (runMode == MODE_MANUAL) { // 5. Auto mode?
        actionManual();             //    NO -> đứng yên
    } else if (sensorFault || lowBattery) { // 6. Fault / low battery?
        actionStopAlarm();          //    YES -> stop motor + safe mode
    } else if (isNight()) {         // 7. Night?
        actionParkEast();           //    YES -> park TL -> sleep
    } else if (abs(diffVal()) >= DEADBAND_THRESHOLD) { // 8. Deadband?
        actionRotate();             //    YES -> 1 nhịp quay về phía sáng
    } else {
        actionHold();               //    NO -> dừng
    }

    monitorBattery();               // 9. Monitor battery & charging
    alarmUpdate();                  // 10. Alarm / buzzer
    motionTick();
    sendTelemetry("update");        // 11. Gửi telemetry 1 lần duy nhất (cuối loop)
    motionTick();

    if (parkSleepPending) nightSleep();

    if (millis() - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
        lastDisplayMs = millis();
        printState();
    }
    handleSerial();
    delay(5);
}
