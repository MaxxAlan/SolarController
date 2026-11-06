/*
 ==========================================================================
   NHOM 4 - SE2044  |  SOLAR TRACKER 2 TRUC  |  ARDUINO UNO
 ==========================================================================
  - 4 LDR   -> tim huong sang nhat -> 2 servo (quay NGANG + quay DOC)
  - INA219  -> dong / ap solar, cong don nang luong (Wh)
  - LCD I2C 20x4: thong tin nhom, ket noi, % pin, dong solar, nang luong
  - Relay   -> tu dong dong/ngat nhanh solar -> TP4056
  - UART (D0/D1) noi chuyen voi ESP32-S3, moi goi la 1 dong text:
        ESP -> UNO : E,<wifi state 0=NOConf 1=Conect 2=OK>,<pin %>,<pin mV>
        ESP -> UNO (lenh dieu khien / config, gui lai khi mat link):
                     M,<mode 0=auto 1=manual>,<panTgt>,<tiltTgt>
                     R,<relay 0=auto 1=on 2=off 3=KHAN CAP>
                     K,<panMin>,<panMax>,<tiltMin>,<tiltMax>,
                       <solarOn mV>,<solarOff mV>,<battStop %>,<battResume %>
        UNO -> ESP : U,<solar mV>,<mA>,<mW>,<mWh>,<goc ngang>,<goc doc>,
                     <relay 0/1>,<mode>,<LDR TL>,<LDR TR>,<LDR BL>,<LDR BR>
                     (so du la relay: 0 tu dong, 1 bat, 2 tat, 3 khan cap)

  Thu vien (Library Manager): "Adafruit INA219" (kem Adafruit BusIO),
                              "LiquidCrystal I2C" (Frank de Brabander).
                              Servo co san.
  Board: Arduino Uno

  Luu y:
   - HOME mac dinh = 0 cho CA HAI servo (co khi da cai lai moi).
   - Mode / relay / gioi han goc dieu khien tu web (via ESP) co uu tien cao
     hon logic auto; mat link ESP > 4 giay UNO tu quay ve auto an toan.
   - KHI NAP CODE CHO UNO: RUT DAY D0 (ESP TX -> UNO RX), NAP XONG CAM LAI.
 ==========================================================================
*/

#include <Wire.h>
#include <Servo.h>
#include <Adafruit_INA219.h>
#include <LiquidCrystal_I2C.h>

/* ======================================================================
 *                              C O N F I G
 *        (chi sua phan nay, phan code phia duoi khong can dung vao)
 * ====================================================================== */

// ---------- 0. Che do test / can chinh --------------------------------
// 1 = LCD hien do sang 4 LDR + goc 2 servo (de kiem tra chan, chieu quay).
// 0 = man hinh binh thuong. Can chinh xong thi dat ve 0.
#define TEST_MODE             0

// ---------- 1. Thong tin nhom (dong 1 tren LCD) ------------------------
#define GROUP_TEXT            "NHOM 4 - SE2044"

// ---------- 2. Servo ---------------------------------------------------
#define PIN_SERVO_PAN         9       // servo QUAY NGANG (trai <-> phai)
#define PIN_SERVO_TILT        10      // servo QUAY DOC   (len  <-> xuong)

// Chieu quay (nhin tu phia SAU tam pin, huong ve mat troi):
//   PAN_REVERSE  = false : goc servo TANG -> tam pin quay sang PHAI
//   TILT_REVERSE = false : goc servo TANG -> tam pin ngang LEN
// Neu tam pin quay NGUOC voi phia co anh sang -> doi false <-> true.
#define PAN_REVERSE           true
#define TILT_REVERSE          false

// Gioi han goc de khong ket khung co khi (admin co the doi qua web).
// HOME MAC DINH = 0 cho CA HAI servo (co khi da cai lai moi):
#define PAN_MIN_DEG           0
#define PAN_MAX_DEG           170     // bot ben trai 30 do (180 -> 150)
#define PAN_HOME_DEG          0
#define TILT_MIN_DEG          0
#define TILT_MAX_DEG          160
#define TILT_HOME_DEG         0

// ---------- 3. Cam bien anh sang LDR (chan AO) -------------------------
// Vi tri tren/duoi/trai/phai: nhin tu phia SAU tam pin ve phia mat troi.
#define PIN_LDR_TL            A2      // Top-Left     (tren - trai)
#define PIN_LDR_TR            A1      // Top-Right    (tren - phai)
#define PIN_LDR_BL            A3      // Bottom-Left  (duoi - trai)
#define PIN_LDR_BR            A0      // Bottom-Right (duoi - phai)

// Module LM393 thuong gap: cang sang thi AO cang GIAM -> true.
// Kiem tra bang TEST_MODE: chieu den vao 1 LDR, so cua no phai TANG.
// Neu so GIAM -> doi true <-> false.
#define LDR_LOWER_IS_BRIGHTER true
#define LDR_VCC_MV            3300    // dien ap cap cho LDR (dang noi chan 3.3V cua UNO)
#define ADC_REF_MV            5000    // dien ap tham chieu ADC cua UNO (mac dinh 5V)
#define LDR_SAMPLES           8       // so lan doc lay trung binh moi LDR

// ---------- 4. Thuat toan bam mat troi ---------------------------------
// Do sang duoc quy ve thang 0 .. LDR_RAW_MAX (=675 khi LDR cap 3.3V)
#define TRACK_INTERVAL_MS     50      // chu ky cap nhat (ms)
#define TRACK_STEP_DEG        1       // moi lan dich bao nhieu do
#define TRACK_TOLERANCE       20      // chenh lech nho hon gia tri nay -> dung yen
#define NIGHT_THRESHOLD       60      // do sang trung binh nho hon -> coi la troi toi
#define NIGHT_RETURN_HOME     true    // toi lien tuc -> ve vi tri HOME
#define NIGHT_WAIT_MS         60000UL // toi bao lau thi ve HOME

// ---------- 5. Relay (dong/ngat nhanh solar -> TP4056) -----------------
#define PIN_RELAY             7
#define RELAY_ACTIVE_LOW      false    // module thuong kich muc THAP. Jumper o vi tri H -> false
#define SOLAR_ON_MV_DEF       3700    // PvPanel >= 3.7V (doc khi CHUA co tai) -> DAT relay ON de nap dien
#define SOLAR_OFF_MV_DEF      3300    // PvPanel <  3.3V (doc khi DANG co tai, ap sag xuong) -> NGAT relay
#define SOLAR_OFF_HOLD_MS     5000UL  // phai thap hon nguong OFF lien tuc bao lau moi ngat (chong ap nhay)
#define BATT_STOP_PCT_DEF     100     // pin >= % nay -> ngat sac (admin doi duoc qua web)
#define BATT_RESUME_PCT_DEF   90      // pin <= % nay -> sac lai   (admin doi duoc qua web)
#define RELAY_MIN_SWITCH_MS   10000UL // toi thieu giua 2 lan dong/ngat (chong nhay relay)
#define RELAY_ON_IF_NO_ESP    true    // mat ket noi ESP (khong biet % pin) -> van cho sac

// ---------- 6. INA219 --------------------------------------------------
#define INA219_ADDR           0x40
#define INA219_RANGE          0       // 0 = 32V/2A | 1 = 32V/1A | 2 = 16V/400mA
#define CURRENT_DEADBAND_MA   3       // dong nho hon gia tri nay xem nhu 0 (khu nhieu)
#define POWER_SAMPLE_MS       250     // chu ky doc INA219 + cong don nang luong

// ---------- 7. LCD I2C -------------------------------------------------
#define LCD_ADDR              0x27    // thuong 0x27 hoac 0x3F
#define LCD_COLS              20
#define LCD_ROWS              4
#define LCD_REFRESH_MS        1000

// ---------- 8. UART noi chuyen voi ESP32-S3 (D0 = RX, D1 = TX) ---------
#define ESP_BAUD              9600    // phai TRUNG voi ESP
#define ESP_LINK_TIMEOUT_MS   4000UL  // qua thoi gian nay khong nhan goi -> bao mat ket noi
#define ESP_SEND_MS           1000    // chu ky gui du lieu sang ESP
#define RX_BUF_LEN            64      // vuot 40 (dong K dai hon) -> dung 64

/* ======================================================================
 *                    C O D E   (khong can sua)
 * ====================================================================== */

#define LDR_RAW_MAX ((int)(1023L * LDR_VCC_MV / ADC_REF_MV))
#define MODE_AUTO       0
#define MODE_MANUAL     1
#define RELAY_CMD_AUTO  0
#define RELAY_CMD_ON    1
#define RELAY_CMD_OFF   2
#define RELAY_CMD_ESTOP 3     // NGAT KHAN CAP - dinh, khong auto BAT lai

Servo servoPan, servoTilt;
Adafruit_INA219 ina219(INA219_ADDR);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// ---- Tracker ----
int  ldrTL = 0, ldrTR = 0, ldrBL = 0, ldrBR = 0;   // do sang, lon = sang
int  panDeg  = PAN_HOME_DEG;
int  tiltDeg = TILT_HOME_DEG;
bool isNight = false;
unsigned long nightSince = 0;

// ---- Gioi han / tham so runtime (ESP co the gui lenh K de doi) ----
int panMinDeg  = PAN_MIN_DEG,  panMaxDeg  = PAN_MAX_DEG;
int tiltMinDeg = TILT_MIN_DEG, tiltMaxDeg = TILT_MAX_DEG;
int  solarOnMv  = SOLAR_ON_MV_DEF;
int  solarOffMv = SOLAR_OFF_MV_DEF;
int  battStopPct   = BATT_STOP_PCT_DEF;
int  battResumePct = BATT_RESUME_PCT_DEF;

// ---- Che do dieu khien (web -> ESP -> UNO) ----
uint8_t mode    = MODE_AUTO;      // 0 = auto, 1 = manual
int     panTgt  = PAN_HOME_DEG;   // muc tieu trong che do manual
int     tiltTgt = TILT_HOME_DEG;

// ---- Relay ----
uint8_t relayCmd = RELAY_CMD_AUTO; // 0 auto, 1 on, 2 off, 3 estop
bool relayOn = false;
bool relayFirst = true;
bool battFull = false;
unsigned long relayLastSwitch = 0;
unsigned long lowVSince = 0;   // bat dau tu khi PvPanel_low (< nguong OFF), 0 = binh thuong

// ---- INA219 / nang luong ----
bool     inaOk = false;
int      solarMv = 0;            // ap solar (mV)
int      solarMa = 0;            // dong solar (mA)
long     solarMw = 0;            // cong suat (mW)
uint32_t energyMwh = 0;          // nang luong cong don (mWh)
uint32_t energyAcc = 0;          // phan le, don vi mW*ms
unsigned long lastPowerMs = 0;

// ---- Du lieu tu ESP ----
int  espWifiSt = 0;            // 0=NOConf 1=Conect 2=OK (tu goi E)
int  battPct = -1;               // -1 = chua biet
int  battMv  = 0;
bool espEverRx = false;
unsigned long espLastRx = 0;
char    rxBuf[RX_BUF_LEN];
uint8_t rxLen = 0;

// ----------------------------------------------------------------------
bool espLinkOk() {
  return espEverRx && (millis() - espLastRx) < ESP_LINK_TIMEOUT_MS;
}

// ---------------------------- RELAY -----------------------------------
void relayWrite(bool on) {
  relayOn = on;
  digitalWrite(PIN_RELAY, (on ^ RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

void updateRelay() {
  unsigned long now = millis();
  bool want = true;

  switch (relayCmd) {
    case RELAY_CMD_ON:    want = true;  break;   // web bat cai dat ON
    case RELAY_CMD_OFF:   want = false; break;   // web bat cai dat OFF
    case RELAY_CMD_ESTOP: want = false; break;   // NGAT KHAN CAP: dinh, auto khong duoc bat lai
    default: {                                           // AUTO
      // 1) Co nang khong?
      //    - Dang NGAT (chua co tai): doc ap that cua panel -> bat khi >= solarOnMv
      //    - Dang DONG (co tai): ap panel SAG xuong vi TP4056 hut dong -> chi
      //      ngat khi ap thap hon solarOffMv LIEN TUC het SOLAR_OFF_HOLD_MS
      //      Neu ngung ngay khi ap < 3.7V thi relay se ngat/lap lai lien tuc
      //      va lam dut mach sac (bug cu).
      bool sunOk = true;
      if (inaOk) {
        if (relayOn) {
          if (solarMv < solarOffMv) {
            if (lowVSince == 0) lowVSince = now;
            sunOk = (now - lowVSince) < SOLAR_OFF_HOLD_MS;
          } else {
            lowVSince = 0;
            sunOk = true;
          }
        } else {
          lowVSince = 0;
          sunOk = (solarMv >= solarOnMv);
        }
      }

      // 2) Pin da day chua? (co tre giua STOP va RESUME)
      bool battKnown = espLinkOk() && battPct >= 0;
      if (battKnown) {
        if (battPct >= battStopPct)         battFull = true;
        else if (battPct <= battResumePct)  battFull = false;
      } else {
        battFull = !RELAY_ON_IF_NO_ESP;
      }
      want = sunOk && !battFull;
      break;
    }
  }

  if (want != relayOn) {
    if (relayFirst || (now - relayLastSwitch) >= RELAY_MIN_SWITCH_MS) {
      relayWrite(want);
      relayLastSwitch = now;
      relayFirst = false;
    }
  }
}

// ---------------------------- INA219 ----------------------------------
void updatePower() {
  unsigned long now = millis();
  unsigned long dt = now - lastPowerMs;
  lastPowerMs = now;
  if (!inaOk) return;
  if (dt > 2000UL) dt = 2000UL;                 // chong tran khi vong lap bi treo

  float busV = ina219.getBusVoltage_V();        // ap tai chan Vin- (phia relay)
  float ma   = ina219.getCurrent_mA();
  float mw   = ina219.getPower_mW();
  if (ma < CURRENT_DEADBAND_MA) { ma = 0; mw = 0; }

  solarMv = (int)(busV * 1000.0f);
  solarMa = (int)(ma + 0.5f);
  solarMw = (long)(mw + 0.5f);

  // Cong don nang luong bang so nguyen: mW*ms, du 3.600.000 thi tang 1 mWh
  energyAcc += (uint32_t)solarMw * dt;
  while (energyAcc >= 3600000UL) {
    energyAcc -= 3600000UL;
    energyMwh++;
  }
}

// ------------------------------ LDR -----------------------------------
int readLdr(uint8_t pin) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < LDR_SAMPLES; i++) sum += analogRead(pin);
  int v = (int)(sum / LDR_SAMPLES);
  if (LDR_LOWER_IS_BRIGHTER) v = LDR_RAW_MAX - v;
  return constrain(v, 0, LDR_RAW_MAX);
}

void readLdrs() {
  ldrTL = readLdr(PIN_LDR_TL);
  ldrTR = readLdr(PIN_LDR_TR);
  ldrBL = readLdr(PIN_LDR_BL);
  ldrBR = readLdr(PIN_LDR_BR);
}

int stepToward(int cur, int target) {
  if (cur < target) return min(cur + TRACK_STEP_DEG, target);
  if (cur > target) return max(cur - TRACK_STEP_DEG, target);
  return cur;
}

void trackSun() {
  readLdrs();

  int oldPan = panDeg, oldTilt = tiltDeg;
  int avgAll = (int)(((long)ldrTL + ldrTR + ldrBL + ldrBR) / 4);
  unsigned long now = millis();

  if (mode == MODE_MANUAL) {
    // THU CONG: di theo muc tieu tu web (buoc nho, di chuyen muot)
    panDeg  = stepToward(panDeg,  constrain(panTgt,  panMinDeg,  panMaxDeg));
    tiltDeg = stepToward(tiltDeg, constrain(tiltTgt, tiltMinDeg, tiltMaxDeg));
  } else if (avgAll < NIGHT_THRESHOLD) {
    // Troi toi: dung bam; toi lau thi ve HOME (= 0)
    if (!isNight) { isNight = true; nightSince = now; }
    if (NIGHT_RETURN_HOME && (now - nightSince) >= NIGHT_WAIT_MS) {
      panDeg  = stepToward(panDeg,  PAN_HOME_DEG);
      tiltDeg = stepToward(tiltDeg, TILT_HOME_DEG);
    }
  } else {
    isNight = false;

    int top   = (ldrTL + ldrTR) / 2;
    int bot   = (ldrBL + ldrBR) / 2;
    int left  = (ldrTL + ldrBL) / 2;
    int right = (ldrTR + ldrBR) / 2;
    int dV = top - bot;        // > 0 : phia tren sang hon
    int dH = right - left;     // > 0 : phia phai sang hon

    if (abs(dH) > TRACK_TOLERANCE) {
      int dir = (dH > 0) ? 1 : -1;
      if (PAN_REVERSE) dir = -dir;
      panDeg = constrain(panDeg + dir * TRACK_STEP_DEG, panMinDeg, panMaxDeg);
    }
    if (abs(dV) > TRACK_TOLERANCE) {
      int dir = (dV > 0) ? 1 : -1;
      if (TILT_REVERSE) dir = -dir;
      tiltDeg = constrain(tiltDeg + dir * TRACK_STEP_DEG, tiltMinDeg, tiltMaxDeg);
    }
  }

  if (panDeg  != oldPan)  servoPan.write(panDeg);
  if (tiltDeg != oldTilt) servoTilt.write(tiltDeg);
}

// ------------------------------ UART ----------------------------------
bool nextField(char*& p, long& out) {
  if (p == NULL) return false;
  char* comma = strchr(p, ',');
  if (comma) *comma = 0;
  out = atol(p);
  p = comma ? comma + 1 : NULL;
  return true;
}

void applyConfig(long a, long b, long c, long d, long e, long f, long g, long h) {
  panMinDeg    = constrain((int)a, 0, 179);
  panMaxDeg    = constrain((int)b, panMinDeg, 179);
  tiltMinDeg   = constrain((int)c, 0, 179);
  tiltMaxDeg   = constrain((int)d, tiltMinDeg, 179);
  solarOnMv    = constrain((int)e, 0, 20000);
  solarOffMv   = constrain((int)f, 0, max(solarOnMv, 1));
  battStopPct  = constrain((int)g, 0, 100);
  battResumePct= constrain((int)h, 0, battStopPct);
  // dua goc hien tai ve trong gioi han moi
  panDeg  = constrain(panDeg,  panMinDeg,  panMaxDeg);
  tiltDeg = constrain(tiltDeg, tiltMinDeg, tiltMaxDeg);
  servoPan.write(panDeg);
  servoTilt.write(tiltDeg);
}

// Goi tu ESP:
//   E,<wifi>,<pin %>,<pin mV>
//   M,<mode>,<panTgt>,<tiltTgt>
//   R,<relayCmd>
//   K,<panMin>,<panMax>,<tiltMin>,<tiltMax>,<solarOn>,<solarOff>,<battStop>,<battResume>
void handleLine(char* s) {
  char* p;
  long w, x, y, z, a, b, c, d;

  if (s[0] == 'E' && s[1] == ',') {
    p = s + 2;
    if (nextField(p, w) && nextField(p, x) && nextField(p, y)) {
      espWifiSt = constrain((int)w, 0, 2);   // 0=NOConf 1=Conect 2=OK
      battPct = constrain((int)x, 0, 100);
      battMv  = (int)y;
      espLastRx = millis();
      espEverRx = true;
    }
    return;
  }

  if (s[0] == 'M' && s[1] == ',') {              // mode + muc tieu goc
    p = s + 2;
    if (nextField(p, w) && nextField(p, x) && nextField(p, y)) {
      uint8_t m = (w == 1) ? MODE_MANUAL : MODE_AUTO;
      if (m != mode) { isNight = false; nightSince = 0; }
      mode   = m;
      panTgt  = (int)x;
      tiltTgt = (int)y;
    }
    return;
  }

  if (s[0] == 'R' && s[1] == ',') {              // relay / NGAT KHAN CAP
    p = s + 2;
    if (nextField(p, w)) {
      relayCmd = (uint8_t)constrain(w, 0, 3);
      relayFirst = true;                         // cho phep ung dung ngay
    }
    return;
  }

  if (s[0] == 'K' && s[1] == ',') {              // config gioi han + relay
    p = s + 2;
    if (nextField(p, a) && nextField(p, b) && nextField(p, c) && nextField(p, d) &&
        nextField(p, w) && nextField(p, x) && nextField(p, y) && nextField(p, z)) {
      applyConfig(a, b, c, d, w, x, y, z);
    }
    return;
  }
  // goi khac -> bo qua
}

void pollEsp() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      rxBuf[rxLen] = 0;
      handleLine(rxBuf);
      rxLen = 0;
    } else if (c != '\r') {
      if (rxLen < sizeof(rxBuf) - 1) rxBuf[rxLen++] = c;
      else rxLen = 0;                           // dong qua dai -> bo
    }
  }
}

// Goi sang ESP:
//   U,<solar mV>,<mA>,<mW>,<mWh>,<goc ngang>,<goc doc>,<relay>,<mode>,
//     <LDR TL>,<LDR TR>,<LDR BL>,<LDR BR>
void sendToEsp() {
  Serial.print(F("U,"));
  Serial.print(solarMv);    Serial.print(',');
  Serial.print(solarMa);    Serial.print(',');
  Serial.print(solarMw);    Serial.print(',');
  Serial.print(energyMwh);  Serial.print(',');
  Serial.print(panDeg);     Serial.print(',');
  Serial.print(tiltDeg);    Serial.print(',');
  Serial.print(relayOn ? 1 : 0); Serial.print(',');
  Serial.print(mode);       Serial.print(',');
  Serial.print(ldrTL);      Serial.print(',');
  Serial.print(ldrTR);      Serial.print(',');
  Serial.print(ldrBL);      Serial.print(',');
  Serial.println(ldrBR);
}

// ------------------------------- LCD ----------------------------------
void lcdRow(uint8_t row, const char* s) {       // ghi 1 dong, tu dong pad cho du 20 ky tu
  char buf[LCD_COLS + 1];
  uint8_t i = 0;
  for (; i < LCD_COLS && s[i]; i++) buf[i] = s[i];
  for (; i < LCD_COLS; i++) buf[i] = ' ';
  buf[LCD_COLS] = 0;
  lcd.setCursor(0, row);
  lcd.print(buf);
}

void lcdCenter(uint8_t row, const char* s) {
  char buf[LCD_COLS + 1];
  uint8_t len = (uint8_t)strlen(s);
  if (len > LCD_COLS) len = LCD_COLS;
  uint8_t left = (LCD_COLS - len) / 2;
  memset(buf, ' ', LCD_COLS);
  memcpy(buf + left, s, len);
  buf[LCD_COLS] = 0;
  lcd.setCursor(0, row);
  lcd.print(buf);
}

void lcdShowNormal() {
  char line[LCD_COLS + 1];
  bool link = espLinkOk();

  // Dong 0: thong tin nhom (manual/khan cap thi uu tien hien trang thai)
  if (relayCmd == RELAY_CMD_ESTOP) {
    lcdCenter(0, "!!! NGAT KHAN CAP");
  } else if (mode == MODE_MANUAL) {
    lcdCenter(0, "CHE DO: THU CONG");
  } else {
    lcdCenter(0, GROUP_TEXT);
  }

  // Dong 1: trang thai ket noi
  //   WiFi: NOConf = chua setup | Conect = dang ket noi / khong co internet | OK = OK
  if (espLinkOk()) {
    const char* st = (espWifiSt == 0) ? "NOConf" :
                     (espWifiSt == 1) ? "Conect" : "OK";
    snprintf(line, sizeof(line), "ESP:OK WiFi:%s", st);
  } else {
    snprintf(line, sizeof(line), "ESP:LOST WiFi:--");
  }
  lcdRow(1, line);

  // Dong 2: % pin va dien ap pin
  if (link && battPct >= 0)
    snprintf(line, sizeof(line), "Pin:%3d%% %d.%02dV", battPct, battMv / 1000, (battMv % 1000) / 10);
  else
    snprintf(line, sizeof(line), "Pin: --%% --.--V");
  lcdRow(2, line);

  // Dong 3: PvPanel + trang thai relay
  //   PvPanel:  >=3.70V  R: ON    (on = dang nap dien cho TP4056)
  if (inaOk)
    snprintf(line, sizeof(line), "PvPanel:%2d.%02dV R:%s",
             solarMv / 1000, (solarMv % 1000) / 10, relayOn ? "ON" : "OFF");
  else
    snprintf(line, sizeof(line), "PvPanel: --.--V R:%s", relayOn ? "ON" : "OFF");
  lcdRow(3, line);
}

void lcdShowTest() {
  char line[LCD_COLS + 1];
  snprintf(line, sizeof(line), "TL:%4d  TR:%4d", ldrTL, ldrTR);       lcdRow(0, line);
  snprintf(line, sizeof(line), "BL:%4d  BR:%4d", ldrBL, ldrBR);       lcdRow(1, line);
  snprintf(line, sizeof(line), "NGANG:%3d DOC:%3d", panDeg, tiltDeg); lcdRow(2, line);
  lcdRow(3, isNight ? "TROI TOI - DUNG BAM" : "DANG BAM MAT TROI");
}

// ------------------------------ SETUP/LOOP -----------------------------
void setup() {
  Serial.begin(ESP_BAUD);

  digitalWrite(PIN_RELAY, RELAY_ACTIVE_LOW ? HIGH : LOW);   // trang thai NGAT truoc khi bat OUTPUT
  pinMode(PIN_RELAY, OUTPUT);
  relayWrite(false);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcdCenter(0, GROUP_TEXT);
  lcdCenter(1, "SOLAR TRACKER");
  lcdCenter(2, "Khoi dong...");

  inaOk = ina219.begin();
  if (inaOk) {
#if INA219_RANGE == 1
    ina219.setCalibration_32V_1A();
#elif INA219_RANGE == 2
    ina219.setCalibration_16V_400mA();
#else
    ina219.setCalibration_32V_2A();
#endif
  }

  servoPan.attach(PIN_SERVO_PAN);
  servoTilt.attach(PIN_SERVO_TILT);
  servoPan.write(PAN_HOME_DEG);   // HOME = 0 (ca 2 servo)
  servoTilt.write(TILT_HOME_DEG);
  panDeg  = PAN_HOME_DEG;
  tiltDeg = TILT_HOME_DEG;

  delay(1500);
  lcd.clear();
  lastPowerMs = millis();
}

void loop() {
  static unsigned long tTrack = 0, tPower = 0, tTx = 0, tLcd = 0;
  unsigned long now = millis();

  pollEsp();

  if (now - tTrack >= TRACK_INTERVAL_MS) { tTrack = now; trackSun(); }
  if (now - tPower >= POWER_SAMPLE_MS)   { tPower = now; updatePower(); updateRelay(); }
  if (now - tTx    >= ESP_SEND_MS)       { tTx = now;    sendToEsp(); }
  if (now - tLcd   >= LCD_REFRESH_MS) {
    tLcd = now;
#if TEST_MODE
    lcdShowTest();
#else
    lcdShowNormal();
#endif
  }
}
