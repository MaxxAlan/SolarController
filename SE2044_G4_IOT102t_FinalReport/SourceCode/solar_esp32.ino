/*
  Remote Solar Panel Management - ESP32-S3 gateway
  UART1: RX = GPIO18 (from Uno D5 through the 200/400 ohm divider), TX = GPIO17 (to Uno D4).
  Battery sense: GPIO1 through the 100k/100k divider (reading x2). On an ESP32 WROOM-32 use GPIO34.
  Cloud: Firebase Realtime Database over REST, no extra library.
  All function names start with gw so they cannot clash with system functions.
*/
#include <Arduino.h>

// ======================= CONFIG =======================
#define CLOUD_ENABLED   0      // 0 = simulator: no WiFi, requests are only printed to USB Serial
#define SIM_BATTERY     1      // 1 = synthetic battery voltage (ADC not simulated)
#define SIM_NET_STATE   1      // value reported to the Uno as Net when CLOUD_ENABLED is 0

#define PIN_UART_RX     18
#define PIN_UART_TX     17
#define PIN_BAT         1

#if CLOUD_ENABLED
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #include <HTTPClient.h>
  const char *WIFI_SSID     = "YOUR_WIFI_NAME";
  const char *WIFI_PASS     = "YOUR_WIFI_PASSWORD";
  const char *FIREBASE_URL  = "https://solarcontroller-1cc93-default-rtdb.asia-southeast1.firebasedatabase.app";  // no trailing slash
  const char *FIREBASE_AUTH = "";   // database secret or token; empty = open test rules
#endif
// ======================================================

struct GwTele {
  char state[12]; float v, i, p, wh; int pan, tilt, relay; bool valid;
};
GwTele tele = {"", 0, 0, 0, 0, 0, 0, 0, false};

float gwBatV = 0;
bool gwNetOk = false, gwCmdInit = false, gwLastEstop = false;
String gwLastMode = "AUTO";
int gwLastPan = 90, gwLastTilt = 90;
uint32_t gwTBat = 0, gwTCloud = 0, gwTHist = 0;

// ---------- prototypes ----------
void gwHandleUnoLine(char *line);
void gwReadUart();
void gwUsbToUno();
float gwReadBattery();
bool gwHttp(const char *method, const char *path, const String &body, String *resp);
String gwJsonField(const String &s, const char *key);
void gwApplyCommands(const String &js);
void gwCloudCycle();

// ---------- UART from Uno ----------
void gwHandleUnoLine(char *line) {
  if (line[0] != 'T') return;
  GwTele t;
  int n = sscanf(line, "T,%11[^,],%f,%f,%f,%f,%d,%d,%d",
                 t.state, &t.v, &t.i, &t.p, &t.wh, &t.pan, &t.tilt, &t.relay);
  if (n == 8) { t.valid = true; tele = t; }
}

void gwReadUart() {
  static char buf[96]; static uint8_t n = 0;
  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '\n') { buf[n] = 0; if (n) gwHandleUnoLine(buf); n = 0; }
    else if (c != '\r' && n < sizeof(buf) - 1) buf[n++] = c;
  }
}

// Type a command such as  C,MODE,MANUAL  or  C,ESTOP,1  in the USB serial monitor to test the Uno
void gwUsbToUno() {
  static char buf[48]; static uint8_t n = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') { buf[n] = 0; if (n && buf[0] == 'C') { Serial1.println(buf); } n = 0; }
    else if (c != '\r' && n < sizeof(buf) - 1) buf[n++] = c;
  }
}

// ---------- battery ----------
float gwReadBattery() {
#if SIM_BATTERY
  return 3.85f + 0.25f * sin(millis() / 90000.0f);
#else
  uint32_t mv = 0;
  for (int i = 0; i < 16; i++) { mv += analogReadMilliVolts(PIN_BAT); delay(2); }
  return (mv / 16.0f) / 1000.0f * 2.0f;
#endif
}

// ---------- tiny JSON helper ----------
String gwJsonField(const String &s, const char *key) {
  String k = String("\"") + key + "\":";
  int i = s.indexOf(k);
  if (i < 0) return "";
  i += k.length();
  while (i < (int)s.length() && (s[i] == ' ' || s[i] == '"')) i++;
  int j = i;
  while (j < (int)s.length() && s[j] != ',' && s[j] != '}' && s[j] != '"') j++;
  return s.substring(i, j);
}

// ---------- HTTP to Firebase (or just print in the simulator) ----------
#if CLOUD_ENABLED
WiFiClientSecure gwClient;

bool gwHttp(const char *method, const char *path, const String &body, String *resp) {
  HTTPClient http;
  String url = String(FIREBASE_URL) + path;
  if (strlen(FIREBASE_AUTH) > 0) url += String("?auth=") + FIREBASE_AUTH;
  if (!http.begin(gwClient, url)) return false;
  http.addHeader("Content-Type", "application/json");
  int code = (strcmp(method, "GET") == 0) ? http.GET() : http.sendRequest(method, body);
  bool ok = (code >= 200 && code < 300);
  if (ok && resp) *resp = http.getString();
  http.end();
  return ok;
}
#else
bool gwHttp(const char *method, const char *path, const String &body, String *resp) {
  Serial.printf("[SIM] %s %s %s\n", method, path, body.c_str());
  if (resp) *resp = "";
  return true;
}
#endif

// ---------- commands coming back from the web page ----------
void gwApplyCommands(const String &js) {
  if (!gwCmdInit) {                       // first read after boot: reset stale commands, do not forward
    gwCmdInit = true;
    gwHttp("PUT", "/commands.json", "{\"mode\":\"AUTO\",\"pan\":90,\"tilt\":90,\"estop\":false}", nullptr);
    gwLastMode = "AUTO"; gwLastPan = 90; gwLastTilt = 90; gwLastEstop = false;
    return;
  }
  String m = gwJsonField(js, "mode");
  String p = gwJsonField(js, "pan"), t = gwJsonField(js, "tilt"), e = gwJsonField(js, "estop");
  bool es = (e == "true" || e == "1");
  if (m.length() && m != gwLastMode) { gwLastMode = m; Serial1.printf("C,MODE,%s\n", m.c_str()); }
  if (p.length() && p.toInt() != gwLastPan) { gwLastPan = p.toInt(); Serial1.printf("C,PAN,%d\n", gwLastPan); }
  if (t.length() && t.toInt() != gwLastTilt) { gwLastTilt = t.toInt(); Serial1.printf("C,TILT,%d\n", gwLastTilt); }
  if (e.length() && es != gwLastEstop) { gwLastEstop = es; Serial1.printf("C,ESTOP,%d\n", es ? 1 : 0); }
}

// ---------- one cloud cycle every 2 s ----------
void gwCloudCycle() {
#if CLOUD_ENABLED
  if (WiFi.status() != WL_CONNECTED) { WiFi.reconnect(); gwNetOk = false; return; }
#endif
  uint32_t ts = (uint32_t)time(nullptr);
  if (ts < 100000UL) ts = 0;               // NTP not synced yet
  bool ok = true;
  char body[256];

  if (tele.valid) {
    int pct = constrain((int)((gwBatV - 3.0f) / 1.2f * 100), 0, 100);
    snprintf(body, sizeof(body),
      "{\"state\":\"%s\",\"v\":%.2f,\"i_ma\":%.1f,\"p_w\":%.3f,\"wh\":%.3f,\"bat_v\":%.2f,\"bat_pct\":%d,"
      "\"pan\":%d,\"tilt\":%d,\"relay\":%d,\"ts\":%lu}",
      tele.state, tele.v, tele.i, tele.p, tele.wh, gwBatV, pct, tele.pan, tele.tilt, tele.relay, (unsigned long)ts);
    ok &= gwHttp("PUT", "/telemetry.json", String(body), nullptr);

    if (millis() - gwTHist >= 10000UL) {
      gwTHist = millis();
      snprintf(body, sizeof(body), "{\"ts\":%lu,\"p_w\":%.3f,\"v\":%.2f,\"i_ma\":%.1f}",
               (unsigned long)ts, tele.p, tele.v, tele.i);
      ok &= gwHttp("POST", "/history.json", String(body), nullptr);
    }
  }

  String resp;
  if (gwHttp("GET", "/commands.json", "", &resp)) gwApplyCommands(resp);
  else ok = false;

#if CLOUD_ENABLED
  gwNetOk = ok;
#else
  gwNetOk = SIM_NET_STATE;
  (void)ok;
#endif
}

// ---------- setup / loop ----------
void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);
#if !SIM_BATTERY
  analogSetPinAttenuation(PIN_BAT, ADC_11db);
#endif
#if CLOUD_ENABLED
  gwClient.setInsecure();                  // demo only: no certificate check
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
#endif
  Serial.println("ESP32 gateway ready");
}

void loop() {
  uint32_t now = millis();
  gwReadUart();
  gwUsbToUno();

  if (now - gwTBat >= 5000UL) {
    gwTBat = now;
    gwBatV = gwReadBattery();
    Serial1.printf("B,%.2f\n", gwBatV);
    Serial1.printf("N,%d\n", gwNetOk ? 1 : 0);
  }
  if (now - gwTCloud >= 2000UL) {
    gwTCloud = now;
    gwCloudCycle();
  }
}
