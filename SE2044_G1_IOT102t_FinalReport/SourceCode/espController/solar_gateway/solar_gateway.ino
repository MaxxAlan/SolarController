/*
 ==========================================================================
   NHOM 1 - SE2044  |  ESP32-S3 GATEWAY  +  FIREBASE
 ==========================================================================
  HANH VI KHI KHOI DONG:
   1. Doc WiFi dang luu trong bo nho (Preferences). Chua co / khong ket noi
      duoc -> mo Access Point "SolarTracker-Setup" (pass 12345678) + captive
      portal: user nhap WiFi moi -> LUU -> ESP TU KHOI DONG LAI.
   2. Ket noi thanh cong (khong doi WiFi) -> ESP chay binh thuong; trang web
      local cua ESP TU CHUYEN (forward) sang DASHBOARD tren Firebase Hosting.

   FIREBASE (thay cac define o khu CONFIG phia sau):
   - RTDB:
        /device/tele : {uno, wifi, rssi, pin_pct, pin_mv, solar_mv, solar_ma,
                        solar_mw, energy_wh, pan, tilt, relay, mode,
                        ldr_tl, ldr_tr, ldr_bl, ldr_br}
        /device/status   : "ONLINE" / "AP_SETUP" / "REBOOTING" ...
        /cmd/mode        : "auto" | "manual"            (user + admin)
        /cmd/pan         : 0..179, muc tieu manual      (user + admin)
        /cmd/tilt        : 0..179, muc tieu manual      (user + admin)
        /cmd/relay       : "auto" | "on" | "off"        (user + admin)
        /cmd/emergency   : true/false TAT KHAN CAP      (user + admin)
        /cmd/cfgPanMin / cfgPanMax / cfgTiltMin / cfgTiltMax
        /cmd/cfgSolarOn/ cfgSolarOff / cfgBattStop / cfgBattResume
                         gioi han goc + nguong relay    (chi ADMIN ghi)
        /cmd/wifiSsid, /cmd/wifiPass, /cmd/wifiTick    doi WiFi (user + admin)
   - Lenh /cmd quan ly theo GIA TRI (khong phai queue). Luu tick vao Preferences
     de chong replay: tick da xu ly roi thi bo qua.

  CAC SUA LOI (so voi ban cu):
   - /device/status "ONLINE" duoc thu lai den khi ghi thanh cong.
   - Mat khau WiFi rong (mang mo) doi duoc tu web.
   - Doi WiFi tu web: thu ket noi WiFi moi truoc, sai thi giu WiFi cu, khong
 restart.
   - Tick doi WiFi luu ngay -> khong thu lai moi giay khi WiFi nhap sai.
   - Kiem tra gioi han goc / nguong truoc khi gui xuong UNO.
   - Escape SSID tren trang portal (chong chen HTML).
   - Bo lenh ghi AP_SETUP (luc do khong bao gio ghi duoc).
   - UART RX buffer 2048 byte: giam mat du lieu khi loop bi chan.
   - Relay doi tu web trong luc khan cap duoc ap dung khi het khan cap.
   - resyncUno() chay ca trong che do portal.

  Thu vien (Library Manager):
   - "Firebase Arduino Client Library for ESP8266 and ESP32" (Mobizt) v4.x
  Board: "ESP32S3 Dev Module".
  Serial monitor 115200 (USB debug). Nap ESP KHONG can rut day UART voi UNO.
 ==========================================================================
*/

#include <DNSServer.h>
#include <Firebase_ESP_Client.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <nvs_flash.h>

/* ======================================================================
 *                              C O N F I G
 *        (chi sua phan nay, phan code phia duoi khong can dung vao)
 * ====================================================================== */

// ---------- 1. Thong tin nhom (hien tren web) --------------------------
#define GROUP_TEXT "NHOM 4 - SE2044"

// ---------- 2. FIREBASE  <<< SUA 4 DONG DUOI THEO PROJECT CUA NHOM >>> --
#define FIREBASE_API_KEY                                                       \
  "AIzaSyCnPlrCSClwOm8tWH7EHEUiFVgKa6rEyq0" // Project settings > General > Web
                                            // API Key
#define FIREBASE_DB_URL                                                        \
  "https://"                                                                   \
  "solarcontroller-1cc93-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define DEVICE_EMAIL                                                           \
  "esp32@nhom4.com" // tai khoan loai "device" tao trong Firebase Auth
#define DEVICE_PASSWORD "123456" // >= 6 ky tu; NEN doi sang mat khau manh

// Link dashboard firebase hosting - trang local cua ESP se forward toi.
#define DASHBOARD_URL "https://solarcontroller-1cc93.web.app/"

// ---------- 3. WIFI CAU HINH SAN (Tu dong ket noi khi bat nguon) --------
#define DEFAULT_WIFI_SSID "hoangmanh-pass_00000000"
#define DEFAULT_WIFI_PASS "00000000"

// ---------- 4. PORTAL (config WiFi khi can cuu ho) ---------------------
#define AP_SSID "SolarTracker-Setup"
#define AP_PASS "12345678"
#define PORTAL_TIMEOUT_MS                                                      \
  (10UL * 60UL * 1000UL) // 10 phut khong ai cau hinh -> thu ket noi WiFi cu lai
#define WIFI_CONNECT_TIMEOUT                                                   \
  15000UL // 15 giay thu ket noi WiFi truoc khi mo portal
#define WIFI_DOWN_PORTAL_MS 60000UL // mat WiFi lien tuc 1 phut -> mo portal

// ---------- 4. UART noi chuyen voi UNO ---------------------------------
#define PIN_UART_RX 18      // nhan tu D1 cua UNO (qua cau chia ap 10k/20k)
#define PIN_UART_TX 17      // gui toi D0 cua UNO
#define UART_BAUD 9600      // phai TRUNG voi UNO
#define UART_RX_BUF 2048    // byte; 2048 ~ 2 giay du lieu o 9600 baud
#define UART_SEND_MS 1000UL // chu ky gui goi sang UNO
#define UNO_TIMEOUT_MS                                                         \
  4000UL                     // qua thoi gian nay khong nhan goi -> bao mat link
#define UNO_RESEND_MS 1000UL // mat link UNO thi gui lai lenh M/R/K cu

// ---------- 5. Do dien ap pin -------------------------------------------
#define PIN_BATT_ADC 4
#define BATT_R_TOP_OHM 100000.0f
#define BATT_R_BOT_OHM 100000.0f
#define BATT_CAL 1.000f
#define ADC_SAMPLES 32
#define BATT_SAMPLE_MS 500UL
#define BATT_FILTER 0.2f

// ---------- 6. History (giam du lieu cu the) -----------------------------
#define HISTORY_SAMPLE_MS (3UL * 60UL * 1000UL) // push /history moi 3 phut (180 giay)

/* ======================================================================
 *                    C O D E   (khong can sua)
 * ====================================================================== */

WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

FirebaseAuth auth;
FirebaseConfig config;
FirebaseData fbdoRead;  // doc /cmd
FirebaseData fbdoWrite; // ghi /device/...

// ---- Du lieu tu UNO ----
struct UnoData {
  long solarMv = 0, solarMa = 0, powerMw = 0;
  unsigned long energyMwh = 0;
  int pan = 0, tilt = 0, relay = 0, mode = 0;
  int ldrTL = 0, ldrTR = 0, ldrBL = 0, ldrBR = 0;
  unsigned long lastMs = 0;
  bool everRx = false;
} uno;

// ---- Pin ----
float battMvF = 0;
bool battInit = false;
int battMv = 0;
int battPct = 0;

// ---- Lenh hien tai sang UNO (web -> RTDB -> ESP -> UNO) ----
String cmdMode = "auto"; // "auto" / "manual"
int cmdPan = 0;          // muc tieu manual
int cmdTilt = 0;
String cmdRelay = "auto"; // "auto" / "on" / "off"
bool cmdEstop = false;    // true = TAT KHAN CAP (dinh; web mo khi khac)

// ---- Config gioi han + nguong relay (admin doi qua web) ----
int cfgPanMin = 0, cfgPanMax = 150;
int cfgTiltMin = 0, cfgTiltMax = 160;
int cfgSolarOn = 3700,
    cfgSolarOff = 3300; // OFF thap hon ON (hysteresis) de chong relay lap lai
                        // khi ap panel sag co tai
int cfgBattStop = 100, cfgBattResume = 90;
bool cfgSent = false; // da gui lenh K cho UNO chua

// ---- Trang thai ----
bool portalMode = false; // dang cap AP cho user config WiFi
unsigned long portalStart = 0;
String ssidCur = "";
String passCur = "";
bool wifiSaved = false; // da luu WiFi trong Preferences chua
bool netOk = false;     // WiFi vao router + check DNS ra internet

char rxBuf[96];
size_t rxLen = 0;

// ======================================================================
//                        WIFI / PREFERENCES
// ======================================================================

bool wifiOk() { return WiFi.status() == WL_CONNECTED; }
bool unoLinkOk() {
  return uno.everRx && (millis() - uno.lastMs) < UNO_TIMEOUT_MS;
}

// Trang thai gui sang UNO (hien tren LCD):
//   0 = NOConf (chua luu / chua setup WiFi)
//   1 = Conect (da luu WiFi nhung dang ket noi / chua truc tuyen Firebase)
//   2 = OK     (ket noi WiFi + truc tuyen Firebase thanh cong)
int wifiState() {
  if (!wifiSaved)
    return 0;
  if (!wifiOk())
    return 1;
  return (netOk && Firebase.ready()) ? 2 : 1;
}

// Sau khi WL_CONNECTED van chua chac co internet (router mat WAN / sai DNS).
// Test internet qua Google (www.google.com). Neu chua co thi 3s thu lai, co roi
// thi 30s check 1 lan.
#define NET_CHECK_MS 30000UL
void checkInternet() {
  static unsigned long tNet = 0;
  if (!wifiOk()) {
    netOk = false;
    return;
  }
  unsigned long now = millis();
  unsigned long interval = netOk ? NET_CHECK_MS : 3000UL;
  if (tNet != 0 && now - tNet < interval)
    return;
  tNet = now;
  IPAddress ip;
  netOk = WiFi.hostByName("www.google.com", ip) > 0;
  Serial.printf("[NET] internet check: %s%s\n", netOk ? "OK" : "KHONG CO",
                netOk ? (" (IP: " + ip.toString() + ")").c_str() : "");
}

// Ket noi WiFi co timeout. Tra ve true neu da co IP.
bool connectWifi(const String &ssid, const String &pass,
                 unsigned long timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long t0 = millis();
  unsigned long lastTx = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
    delay(100);
    pollUno();
    if (millis() - lastTx >= 500) {
      lastTx = millis();
      sendToUno(); // Gui sang UNO E,1,... de LCD luon hien 'WiFi:Conect'
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    IPAddress ip;
    netOk = (WiFi.hostByName("www.google.com", ip) > 0);
    return true;
  }
  return false;
}

void saveWifiPrefs(const String &ssid, const String &pass) {
  bool b = prefs.begin("wifi", false);
  if (!b) {
    Serial.println("[PREFS] LOI: prefs.begin() failed!");
  }
  size_t s1 = prefs.putString("ssid", ssid);
  size_t s2 = prefs.putString("pass", pass);
  size_t s3 = prefs.putBool("set", true);
  prefs.end();
  Serial.printf(
      "[PREFS] Da ghi vao Flash: ssid=%d byte, pass=%d byte, set=%d\n", (int)s1,
      (int)s2, (int)s3);
}

bool loadWifiPrefs(String &ssid, String &pass, bool &isSet) {
  bool b = prefs.begin("wifi", true);
  if (!b) {
    Serial.println("[PREFS] LOI: load prefs.begin() failed!");
  }
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  isSet = prefs.getBool("set", false);
  prefs.end();
  Serial.printf("[PREFS] Doc Flash: isSet=%d, ssid='%s'\n", isSet ? 1 : 0,
                ssid.c_str());
  // Neu ssid co chuoi 'pass_' thi la do lan truoc nguoi dung copy nham ca
  // password vao o ssid -> bo qua
  if (ssid.indexOf("pass_") >= 0) {
    return false;
  }
  return isSet && ssid.length() > 0;
}

void saveTick(long tick) {
  prefs.begin("wifi", false);
  prefs.putLong("tick", tick);
  prefs.end();
}

// ======================================================================
//                              PIN
// ======================================================================

const uint16_t BATT_TABLE_MV[] = {3300, 3610, 3690, 3710, 3730, 3750, 3770,
                                  3790, 3800, 3820, 3840, 3850, 3870, 3910,
                                  3950, 3980, 4020, 4080, 4110, 4150, 4200};
const uint8_t BATT_TABLE_PCT[] = {0,  5,  10, 15, 20, 25, 30, 35, 40, 45, 50,
                                  55, 60, 65, 70, 75, 80, 85, 90, 95, 100};

int battPercent(float mv) {
  const int n = sizeof(BATT_TABLE_MV) / sizeof(BATT_TABLE_MV[0]);
  if (mv <= BATT_TABLE_MV[0])
    return 0;
  if (mv >= BATT_TABLE_MV[n - 1])
    return 100;
  for (int i = 1; i < n; i++) {
    if (mv <= BATT_TABLE_MV[i]) {
      float f = (mv - BATT_TABLE_MV[i - 1]) /
                (float)(BATT_TABLE_MV[i] - BATT_TABLE_MV[i - 1]);
      return (int)(BATT_TABLE_PCT[i - 1] +
                   f * (BATT_TABLE_PCT[i] - BATT_TABLE_PCT[i - 1]) + 0.5f);
    }
  }
  return 100;
}

float readBatteryMv() {
  uint32_t sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogReadMilliVolts(PIN_BATT_ADC);
    delayMicroseconds(100);
  }
  float pinMv = sum / (float)ADC_SAMPLES;
  return pinMv * (BATT_R_TOP_OHM + BATT_R_BOT_OHM) / BATT_R_BOT_OHM * BATT_CAL;
}

void updateBattery() {
  float mv = readBatteryMv();
  battMvF = battInit ? battMvF + BATT_FILTER * (mv - battMvF) : mv;
  battInit = true;
  battMv = (int)(battMvF + 0.5f);
  battPct = battPercent(battMvF);
}

// ======================================================================
//                        UART: UNO <-> nhan
// ======================================================================

// Goi tu UNO:  U,<solar mV>,<mA>,<mW>,<mWh>,<pan>,<tilt>,<relay>,<mode>,
//                <ldrTL>,<ldrTR>,<ldrBL>,<ldrBR>
void handleLine(const char *s) {
  long a, b, c, d, e, f, g, h, i, j, k, l;
  int n = sscanf(s, "U,%ld,%ld,%ld,%lu,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld", &a, &b,
                 &c, (unsigned long *)&d, &e, &f, &g, &h, &i, &j, &k, &l);
  if (n >= 7) {
    uno.solarMv = a;
    uno.solarMa = b;
    uno.powerMw = c;
    uno.energyMwh = (unsigned long)d;
    uno.pan = (int)e;
    uno.tilt = (int)f;
    uno.relay = (int)g;
    if (n >= 8)
      uno.mode = (int)h;
    if (n >= 12) {
      uno.ldrTL = (int)i;
      uno.ldrTR = (int)j;
      uno.ldrBL = (int)k;
      uno.ldrBR = (int)l;
    }
    uno.lastMs = millis();
    uno.everRx = true;
  }
}

void pollUno() {
  while (Serial1.available()) {
    char ch = (char)Serial1.read();
    if (ch == '\n') {
      rxBuf[rxLen] = 0;
      handleLine(rxBuf);
      rxLen = 0;
    } else if (ch != '\r') {
      if (rxLen < sizeof(rxBuf) - 1)
        rxBuf[rxLen++] = ch;
      else
        rxLen = 0;
    }
  }
}

// Goi sang UNO:  E,<wifi state 0/1/2>,<pin %>,<pin mV>
void sendToUno() {
  Serial1.printf("E,%d,%d,%d\n", wifiState(), battPct, battMv);
}

// --------------------------- LENH SANG UNO ----------------------------
//   M,<mode 0/1>,<panTgt>,<tiltTgt>
//   R,<0 auto | 1 on | 2 off | 3 NGAT KHAN CAP>
//   K,<panMin>,<panMax>,<tiltMin>,<tiltMax>,<solarOn>,<solarOff>,<battStop>,<battResume>
void sendModeCmd() {
  Serial1.printf("M,%d,%d,%d\n", cmdMode == "manual" ? 1 : 0, cmdPan, cmdTilt);
}

void sendRelayCmd() {
  uint8_t r;
  if (cmdEstop)
    r = 3;
  else if (cmdRelay == "on")
    r = 1;
  else if (cmdRelay == "off")
    r = 2;
  else
    r = 0;
  Serial1.printf("R,%d\n", r);
}

void sendConfigCmd() {
  Serial1.printf("K,%d,%d,%d,%d,%d,%d,%d,%d\n", cfgPanMin, cfgPanMax,
                 cfgTiltMin, cfgTiltMax, cfgSolarOn, cfgSolarOff, cfgBattStop,
                 cfgBattResume);
  cfgSent = true;
}

// Kiem tra cac cap gioi han co hop le khong.
bool cfgValid() {
  return cfgPanMin <= cfgPanMax && cfgTiltMin <= cfgTiltMax &&
         cfgSolarOff <= cfgSolarOn;
}

// Khi UNO vua link lai (hoac vua nap lai / mat dien quay lai) -> gui toan bo
// lenh cu cho UNO, de trang thai (mode / relay / gioi han goc / ngat khan cap)
// khong bi mat.
void resyncUno() {
  static unsigned long lastResend = 0;
  unsigned long now = millis();
  if (!unoLinkOk() && (now - lastResend) >= UNO_RESEND_MS) {
    lastResend = now;
    cfgSent = false;
    sendModeCmd();
    sendRelayCmd();
    sendConfigCmd();
  }
}

// ======================================================================
//                            FIREBASE
// ======================================================================

void tokenStatusCallback(TokenInfo info) {
  Serial.printf("[FB] token: type=%d, status=%d, msg: %s\n", info.type,
                info.status, info.error.message.c_str());
  if (info.status == token_status_ready) {
    Serial.println("[FB] Token READY! San sang ket noi Firebase RTDB!");
  }
}

void firebaseInit() {
  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_DB_URL;
  config.token_status_callback = tokenStatusCallback;

  auth.user.email = DEVICE_EMAIL;
  auth.user.password = DEVICE_PASSWORD;

  fbdoWrite.setBSSLBufferSize(2048, 512);
  fbdoRead.setBSSLBufferSize(4096, 512);
  fbdoRead.setResponseSize(2048);
  fbdoWrite.setResponseSize(2048);

  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
}

// Tra ve true neu ghi thanh cong.
bool setStatus(const String &s) {
  if (wifiOk() && Firebase.ready())
    return Firebase.RTDB.setString(&fbdoWrite, "/device/status", s);
  return false;
}

// Thu ghi "ONLINE" den khi thanh cong (khi WiFi len lai thi ghi lai).
// Thu moi 5 giay de khong ghi lien tuc khi Firebase loi.
void ensureOnlineStatus() {
  static bool sent = false;
  static unsigned long tTry = 0;
  if (!wifiOk()) {
    sent = false;
    return;
  }
  if (sent)
    return;
  unsigned long now = millis();
  if (tTry != 0 && now - tTry < 5000UL)
    return;
  tTry = now;
  sent = setStatus("ONLINE");
}

// ======================================================================
//                    DOC /cmd  (poll giua web + UNO.state)
// ======================================================================

void handleWifiChange(int tick) {
  String lastSsid, lastPass;
  bool isSet;
  loadWifiPrefs(lastSsid, lastPass, isSet);

  FirebaseJson &j = fbdoRead.jsonObject();
  FirebaseJsonData d;
  String ns, np;
  if (j.get(d, "wifiSsid") && d.success)
    ns = d.stringValue;
  // Mat khau rong: RTDB khong luu key rong -> khong co key thi np = ""
  if (j.get(d, "wifiPass") && d.success)
    np = d.stringValue;

  // Luu tick truoc de khong thu lai moi giay neu WiFi nhap sai
  saveTick(tick);

  if (ns.length() == 0)
    return;
  if (ns == lastSsid && np == lastPass)
    return; // khong doi gi

  // Thu ket noi WiFi moi truoc. Sai -> quay lai WiFi cu, khong luu.
  Serial.printf("[WIFI] Thu WiFi moi: %s\n", ns.c_str());
  if (!connectWifi(ns, np, WIFI_CONNECT_TIMEOUT)) {
    Serial.println("[WIFI] WiFi moi that bai -> quay lai WiFi cu, KHONG luu");
    connectWifi(ssidCur, passCur, WIFI_CONNECT_TIMEOUT);
    return;
  }

  // Ket noi duoc -> luu va khoi dong lai de Firebase khoi tao lai tren mang moi
  saveWifiPrefs(ns, np);
  Serial.println("[WIFI] WiFi moi OK -> khoi dong lai");
  setStatus("REBOOTING - WIFI CHANGED");
  delay(1500);
  ESP.restart();
}

void pollCommands() {
  static unsigned long tCmd = 0;
  unsigned long now = millis();
  if (!Firebase.ready() || !wifiOk() || (now - tCmd) < 1000UL)
    return;
  tCmd = now;

  if (!Firebase.RTDB.getJSON(&fbdoRead, "/cmd"))
    return; // chua co du lieu

  FirebaseJson &j = fbdoRead.jsonObject();
  FirebaseJsonData d;
  String s;
  int i;
  bool b;
  bool cmdChanged = false;

  // --- che do auto / manual ---
  if (j.get(d, "mode") && d.success) {
    s = d.stringValue;
    s.toLowerCase();
    if (s != "manual")
      s = "auto";
    if (s != cmdMode) {
      cmdMode = s;
      sendModeCmd();
      cmdChanged = true;
    }
  }

  // --- goc muc tieu (manual) ---
  if (j.get(d, "pan") && d.success) {
    i = (int)d.intValue;
    int v = constrain(i, 0, 179);
    if (v != cmdPan) {
      cmdPan = v;
      if (cmdMode == "manual") {
        sendModeCmd();
        cmdChanged = true;
      }
    }
  }
  if (j.get(d, "tilt") && d.success) {
    i = (int)d.intValue;
    int v = constrain(i, 0, 179);
    if (v != cmdTilt) {
      cmdTilt = v;
      if (cmdMode == "manual") {
        sendModeCmd();
        cmdChanged = true;
      }
    }
  }

  // --- TAT KHAN CAP (dinh; chi mo khi web KHOI PHUC -> emergency=false) ---
  if (j.get(d, "emergency") && d.success) {
    b = d.boolValue;
    if (b != cmdEstop) {
      cmdEstop = b;
      sendRelayCmd();
      cmdChanged = true;
    }
  }

  // --- relay auto/on/off: luon ghi nhan; chi gui xuong UNO khi khong khan cap
  // ---
  if (j.get(d, "relay") && d.success) {
    s = d.stringValue;
    s.toLowerCase();
    if (s != "on" && s != "off")
      s = "auto";
    if (s != cmdRelay) {
      cmdRelay = s;
      if (!cmdEstop) {
        sendRelayCmd();
        cmdChanged = true;
      }
    }
  }

  // --- config gioi han goc + nguong relay (admin) ---
  bool anyCfg = false;
  struct Cfg {
    const char *k;
    int *v;
    int lo;
    int hi;
  };
  Cfg cfgs[8] = {
      {"cfgPanMin", &cfgPanMin, 0, 179},
      {"cfgPanMax", &cfgPanMax, 0, 179},
      {"cfgTiltMin", &cfgTiltMin, 0, 179},
      {"cfgTiltMax", &cfgTiltMax, 0, 179},
      {"cfgSolarOn", &cfgSolarOn, 0, 25000},
      {"cfgSolarOff", &cfgSolarOff, 0, 25000},
      {"cfgBattStop", &cfgBattStop, 0, 100},
      {"cfgBattResume", &cfgBattResume, 0, 100},
  };
  int oldCfg[8];
  for (uint8_t n = 0; n < 8; n++)
    oldCfg[n] = *cfgs[n].v;

  for (uint8_t n = 0; n < 8; n++) {
    if (j.get(d, cfgs[n].k) && d.success) {
      int v = constrain((int)d.intValue, cfgs[n].lo, cfgs[n].hi);
      if (v != *cfgs[n].v) {
        *cfgs[n].v = v;
        anyCfg = true;
      }
    }
  }
  // Gioi han sai (min > max, hoac solarOff > solarOn) -> hoan lai gia tri cu
  if (anyCfg && !cfgValid()) {
    for (uint8_t n = 0; n < 8; n++)
      *cfgs[n].v = oldCfg[n];
    anyCfg = false;
    Serial.println("[CFG] Gioi han khong hop le -> bo qua");
  }
  if (anyCfg && cfgSent)
    sendConfigCmd();

  static bool cmdInitDone = false;
  if (!cmdInitDone) {
    cmdInitDone = true; // Lan dau sync trang thai ban dau tu Firebase, khong
                        // tinh la nhan lenh moi
  } else if (cmdChanged || anyCfg) {
    Serial.println(
        "Acpt"); // Chi in khi nguoi dung thuc su bam thay doi tren Web
  }

  // --- WiFi doi qua web (tick chong replay) ---
  if (j.get(d, "wifiTick") && d.success && d.intValue > 0) {
    prefs.begin("wifi", true);
    long lastTick = prefs.getLong("tick", 0);
    prefs.end();
    if ((long)d.intValue != lastTick)
      handleWifiChange((int)d.intValue);
  }
}

// ======================================================================
//                   TELEMETRY len /device/tele + /history
// ======================================================================

void pushTelemetry() {
  static unsigned long tPush = 0;
  static unsigned long tWait = 0;
  unsigned long now = millis();

  if (!wifiOk())
    return;

  if (!Firebase.ready()) {
    if (now - tWait > 3000UL) {
      tWait = now;
      Serial.println("[FB] Dang cho Firebase.ready()...");
    }
    return;
  }

  if ((now - tPush) < 1000UL)
    return;
  tPush = now;

  bool unoOk = unoLinkOk();
  FirebaseJson j;
  j.set("uno", unoOk ? 1 : 0);
  j.set("wifi", wifiOk() ? 1 : 0);
  j.set("rssi", (int)WiFi.RSSI());
  j.set("pin_pct", battPct);
  j.set("pin_mv", battMv);
  if (unoOk) {
    j.set("solar_mv", uno.solarMv);
    j.set("solar_ma", uno.solarMa);
    j.set("solar_mw", uno.powerMw);
    j.set("energy_wh", uno.energyMwh / 1000.0);
    j.set("pan", uno.pan);
    j.set("tilt", uno.tilt);
    j.set("relay", uno.relay);
    j.set("mode", uno.mode);
    j.set("ldr_tl", uno.ldrTL);
    j.set("ldr_tr", uno.ldrTR);
    j.set("ldr_bl", uno.ldrBL);
    j.set("ldr_br", uno.ldrBR);
  }
  j.set("ms", (long)now);

  if (Firebase.RTDB.setJSON(&fbdoWrite, "/device/tele", &j)) {
    static unsigned long tOkLog = 0;
    if (now - tOkLog > 4000UL) {
      tOkLog = now;
      Serial.println("[FB] Push /device/tele THANH CONG!");
    }
  } else {
    Serial.printf("[FB] tele ERROR: %s (code: %d)\n",
                  fbdoWrite.errorReason().c_str(), fbdoWrite.httpCode());
  }

  // Luu ban ghi /history moi 5 phut (do thi dai han tren web)
  static unsigned long tHist = 0;
  if (unoOk && (now - tHist) >= HISTORY_SAMPLE_MS) {
    tHist = now;
    FirebaseJson h;
    h.set("ms", (long)now);
    h.set("pin_pct", battPct);
    h.set("solar_mv", uno.solarMv);
    h.set("solar_mw", uno.powerMw);
    h.set("energy_wh", uno.energyMwh / 1000.0);
    if (!Firebase.RTDB.pushJSON(&fbdoWrite, "/history", &h))
      Serial.printf("[FB] history ERROR: %s\n",
                    fbdoWrite.errorReason().c_str());
  }
}

// ======================================================================
//                       WEB: PORTAL  +  LOCAL PAGE
// ======================================================================

// Escape cac ky tu dac biet HTML (de SSID khong chen duoc ma vao trang)
String htmlEscape(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
    case '<':
      o += "&lt;";
      break;
    case '>':
      o += "&gt;";
      break;
    case '&':
      o += "&amp;";
      break;
    case '"':
      o += "&quot;";
      break;
    case '\'':
      o += "&#39;";
      break;
    default:
      o += c;
    }
  }
  return o;
}

// ------------------------ trang config WiFi (AP mode) -----------------
const char PORTAL_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NHOM 4 - SE2044 | Cau hinh WiFi</title>
<style>
body{font-family:sans-serif;background:#111;color:#eee;margin:0;padding:20px;max-width:420px;margin:auto}
h1{font-size:18px}
.c{background:#222;border-radius:10px;padding:16px;margin:12px 0}
input{width:100%;padding:10px;margin:8px 0;box-sizing:border-box;border-radius:6px;border:1px solid #555;background:#111;color:#eee}
button{width:100%;padding:12px;border:0;border-radius:8px;background:#0a84ff;color:#fff;font-size:15px}
small{color:#9ab}
</style></head><body>
<h1>NHOM 4 - SE2044 | Solar Tracker</h1>
<div class="c">
  <b>Cau hinh WiFi</b><br>
  <small>Nhap WiFi can ket noi, bam LUU. Thiet bi se tu khoi dong lai va noi WiFi moi.</small>
  <form action="/save" method="post">
    <input name="ssid" placeholder="Ten WiFi (SSID)" required>
    <input name="pass" type="password" placeholder="Mat khau WiFi">
    <button type="submit">LUU VA KHOI DONG LAI</button>
  </form>
</div>
</body></html>
)rawliteral";

void portalSave() {
  String ssid = server.hasArg("ssid") ? server.arg("ssid") : "";
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  Serial.printf("[PORTAL] Nhan luu WiFi: SSID='%s' Pass='%s'\n", ssid.c_str(),
                pass.c_str());

  if (ssid.length() == 0) {
    Serial.println("[PORTAL] Loi: SSID trong!");
    server.send(400, "text/html", "Vui long nhap SSID!");
    return;
  }

  // So sanh voi cau hinh WiFi cu trong bo nho de tranh luu bua va khoi dong lai
  // vo ly
  String oldSsid, oldPass;
  bool isSet = false;
  loadWifiPrefs(oldSsid, oldPass, isSet);
  if (isSet && oldSsid == ssid && oldPass == pass) {
    Serial.println("[PORTAL] Cau hinh trung khop WiFi hien tai, bo qua.");
    String html =
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta http-equiv='refresh' content='3;url=/setup'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Khong doi</title>"
        "</head><body "
        "style='font-family:sans-serif;background:#111;color:#eee;padding:24px;"
        "text-align:center'>"
        "<h3 style='color:#ff9800'>WiFi nay da duoc luu truoc do!</h3>"
        "<p>Khong can luu lai.</p>"
        "<p><a href='/setup' style='color:#64b5f6'>Quay lai</a></p>"
        "</body></html>";
    server.send(200, "text/html", html);
    return;
  }

  // Co su thay doi cau hinh -> chap nhan va luu
  Serial.println("Acpt");
  saveWifiPrefs(ssid, pass);
  Serial.println("[PORTAL] Da luu cau hinh moi vao Preferences! Dang khoi dong "
                 "lai sau 3 giay...");

  String html =
      "<!DOCTYPE html><html><head><meta charset='utf-8'>"
      "<meta http-equiv='refresh' content='4;url=" DASHBOARD_URL "'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Da luu WiFi</title>"
      "<script>setTimeout(function(){ window.location.href = '" DASHBOARD_URL
      "'; }, 3500);</script>"
      "</head><body "
      "style='font-family:sans-serif;background:#111;color:#eee;padding:24px;"
      "text-align:center'>"
      "<h2 style='color:#4caf50'>&#10004; Da luu WiFi: " +
      htmlEscape(ssid) +
      "</h2>"
      "<p>ESP32 dang khoi dong lai de ket noi mang va truc tuyen...</p>"
      "<p style='color:#aaa;font-size:14px'>Trinh duyet se tu dong chuyen sang "
      "Dashboard trong giay lat.</p>"
      "<p><a href='" DASHBOARD_URL
      "' style='color:#64b5f6'>Bam vao day neu khong tu chuyen</a></p>"
      "</body></html>";
  server.send(200, "text/html", html);
  delay(3000);
  ESP.restart();
}

void portalPage() { server.send(200, "text/html", PORTAL_PAGE); }
void portalRedirectAll() {
  server.sendHeader("Location",
                    "http://" + WiFi.softAPIP().toString() + "/setup", true);
  server.send(302, "text/plain", "");
}

void startPortal() {
  portalMode = true;
  portalStart = millis();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);
  Serial.printf("[AP] Portal WiFi dang chay. SSID=%s Pass=%s | IP=%s\n",
                AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.begin();
  // HTTP handler da duoc dang ky MOT LAN trong setup() (registerHttpHandlers).
  Serial.println(
      "[PORTAL] Vao WiFi 'SolarTracker-Setup' (12345678) de cau hinh");
}

// ------------------------ trang local (da co WiFi) --------------------
// Trang local TU CHUYEN (forward) sang DASHBOARD firebase sau 2 giay.
const char LOCAL_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta http-equiv="refresh" content="2;url=DASHBOARD_URL">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NHOM 4 - SE2044</title>
<script>location.replace("DASHBOARD_URL");</script>
</head><body style="font-family:sans-serif;background:#111;color:#eee;padding:20px">
<div>Dang chuyen den Dashboard Firebase... <a href="DASHBOARD_URL">Bam vao day neu khong tu chuyen</a></div>
</body></html>
)rawliteral";

void handleLocalRoot() {
  String page = FPSTR(LOCAL_PAGE);
  page.replace("DASHBOARD_URL", DASHBOARD_URL);
  server.send(200, "text/html", page);
}

void startLocalServer() {
  server.begin();
  Serial.printf("[WEB] Local web server chay, IP = %s (tu forward sang %s)\n",
                WiFi.localIP().toString().c_str(), DASHBOARD_URL);
}

// ---------------- DANG KY HTTP MOT LAN (ngay khi boot) -----------------
// Moi handler dispatch theo co portalMode -> portal VA local server dung chung
// mot bo handler, khong con trung "/".
void httpRoot() {
  if (portalMode)
    portalPage();
  else
    handleLocalRoot();
}
void httpSave() {
  if (portalMode)
    portalSave();
  else
    handleLocalRoot();
}
void httpCaptive() {
  if (portalMode)
    portalRedirectAll();
  else
    handleLocalRoot();
}

void registerHttpHandlers() {
  server.on("/", httpRoot);
  server.on("/setup", httpRoot);
  server.on(
      "/save",
      httpSave); // chap nhan ca GET va POST de tranh loi trinh duyet captive
  // Captive portal: redirect moi URL nhu Google / Apple check vao trang config
  server.on("/generate_204", httpCaptive);
  server.on("/hotspot-detect.html", httpCaptive);
  server.on("/connecttest.txt", httpCaptive);
  server.on("/wpad.dat", httpCaptive);
  server.onNotFound(httpCaptive);
}

// ======================================================================
//                        SETUP / LOOP
// ======================================================================

void setup() {
  Serial.begin(115200);
  delay(200);

  // Khoi tao phan vung NVS de Preferences luu vao Flash khong bi loi
  esp_err_t nvsErr = nvs_flash_init();
  if (nvsErr == ESP_ERR_NVS_NO_FREE_PAGES ||
      nvsErr == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  Serial1.begin(UART_BAUD, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);
  Serial1.setRxBufferSize(
      UART_RX_BUF); // goi SAU begin() de tranh assert fail xQueueSemaphoreTake

  analogSetPinAttenuation(PIN_BATT_ADC, ADC_11db);
  updateBattery();

  registerHttpHandlers(); // dang ky HTTP 1 lan — truoc khi chon portal / local

  // UU TIEN SO 1: Kiem tra WiFi trong Flash. Neu chua co / loi, lay ngay WiFi
  // mac dinh cau hinh san
  String ssid, pass;
  bool isSet = false;
  if (loadWifiPrefs(ssid, pass, isSet) && isSet && ssid.length() > 0) {
    ssidCur = ssid;
    passCur = pass;
    Serial.printf("[WIFI] Doc tu Flash: SSID='%s'\n", ssidCur.c_str());
  } else {
    ssidCur = DEFAULT_WIFI_SSID;
    passCur = DEFAULT_WIFI_PASS;
    Serial.printf("[WIFI] Dung WiFi cau hinh san: SSID='%s' Pass='%s'\n",
                  ssidCur.c_str(), passCur.c_str());
    saveWifiPrefs(ssidCur, passCur);
  }

  wifiSaved = true;
  WiFi.mode(WIFI_STA);
  Serial.printf("[WIFI] Bat dau thu ket noi: %s...\n", ssidCur.c_str());

  // Gui 1 goi sang UNO de LCD hien WiFi:Conect ngay lap tuc
  sendToUno();

  if (connectWifi(ssidCur, passCur, WIFI_CONNECT_TIMEOUT)) {
    Serial.printf("[WIFI] >> KET NOI THANH CONG! IP=%s RSSI=%d dBm\n",
                  WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
    Serial.println("[WIFI] Ket noi san co thanh cong -> BO QUA PORTAL SETUP!");

    startLocalServer();
    firebaseInit();

    // Gui sang UNO goc home mac dinh (chua nhan duoc gia tri web)
    cmdPan = 0;
    cmdTilt = 0;
    sendModeCmd();   // auto
    sendRelayCmd();  // auto
    sendConfigCmd(); // gioi han + nguong relay theo define
    return;
  }

  // Chi khi ket noi WiFi san co that bai sau timeout -> Moi mo Portal AP de cuu
  // ho
  Serial.println("[WIFI] Ket noi that bai -> Mo Portal AP SolarTracker-Setup");
  wifiSaved = false;
  startPortal();
}

void loop() {
  unsigned long now = millis();
  static unsigned long tBatt = 0, tTx = 0, tDown = 0, tRetry = 0;

  // ---- PORTAL MODE ----
  if (portalMode) {
    dnsServer.processNextRequest();
    server.handleClient();

    // UART + pin VAN CHAY trong portal: UNO transparent hien % pin
    // (pin% do ESP tu do ADC, khong can WiFi)
    static unsigned long tBattP = 0, tTxP = 0;
    unsigned long np = millis();
    pollUno();
    if (np - tBattP >= BATT_SAMPLE_MS) {
      tBattP = np;
      updateBattery();
    }
    if (np - tTxP >= UART_SEND_MS) {
      tTxP = np;
      sendToUno();
    }
    resyncUno(); // UNO reset trong luc portal van nhan lai lenh

    if (millis() - portalStart > PORTAL_TIMEOUT_MS) {
      Serial.println(
          "[PORTAL] Het thoi gian cau hinh -> khoi dong lai thu WiFi cu");
      ESP.restart();
    }
    return;
  }

  // ---- MODE BINH THUONG ----
  server.handleClient();
  pollUno();

  if (now - tBatt >= BATT_SAMPLE_MS) {
    tBatt = now;
    updateBattery();
  }
  if (now - tTx >= UART_SEND_MS) {
    tTx = now;
    sendToUno();
  }

  // WiFi mat ket noi: thu lai; 1 phut lien tuc -> mo portal
  if (!wifiOk()) {
    if (tDown == 0) {
      tDown = now;
    }
    if (now - tRetry >= 10000UL) {
      tRetry = now;
      WiFi.disconnect();
      WiFi.begin(ssidCur.c_str(), passCur.c_str());
    }
    if (now - tDown >= WIFI_DOWN_PORTAL_MS) {
      Serial.println("[WIFI] Mat ket noi qua lau -> mo portal config");
      tDown = 0;
      startPortal();
      return;
    }
  } else {
    tDown = 0;
    tRetry = 0;
  }

  // UNO mat link (do UNO vua nap lai / mat dien) -> gui lai lenh cu
  resyncUno();

  // Firebase: doc lenh /cmd + day du lieu
  checkInternet();
  pollCommands();
  pushTelemetry();
  ensureOnlineStatus();
}
