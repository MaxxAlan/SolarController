# SOLAR TRACKER — NHOM 1 SE2044 — Final build

Hardware: Arduino UNO (sun tracker) + ESP32-S3 (gateway) + Firebase (Auth, RTDB, Hosting).
The dashboard runs on Firebase Hosting — the ESP only bridges UNO <-> RTDB.

> **The ESP32-S3 never hosts a website itself.** It is a WiFi *client*: it joins
> your router, pushes telemetry to Firebase RTDB and pulls `/cmd`. Anything the
> outside world opens lives on Firebase Hosting
> (`https://solarcontroller-1cc93.web.app/`). So "ESP not on the internet" always
> means "ESP not joined to WiFi" — see the walkthrough in section 2.1.

```
SourceCode/
├─ unoController/uno_solar_tracker/uno_solar_tracker.ino   # firmware UNO
├─ espController/solar_gateway/solar_gateway.ino           # firmware ESP32-S3
├─ webController/
│   ├─ public/               index.html (login) + dashboard.html + dashboard.js + firebase-config.js
│   ├─ firebase.json         hosting config
│   ├─ .firebaserc           project id (solarcontroller-1cc93)
│   ├─ database.rules.json   role-based rules
│   └─ seed/seed.js          create 3 accounts + roles
└─ README.md
```

## 1. UNO controller

Libraries (Arduino Library Manager): **Adafruit INA219** (with Adafruit BusIO),
**LiquidCrystal I2C** (Frank de Brabander), Servo (built-in).

Configs live in the .ino header: servo PAN/TILT (D9/D10), PAN_REVERSE/TILT_REVERSE,
angle limits (UNO defaults PAN 0..150, TILT 0..90 — the ESP immediately overrides
them with the web defaults **PAN 0..150, TILT 0..90** via the `K` command, and the
admin can change both from the dashboard CONFIG panel), **HOME = 0 for both servos**,
LDR pins A2/A1/A3/A0, LDR_LOWER_IS_BRIGHTER, tracking params, relay D7, LCD 0x27,
UART 9600.

Flashing UNO: **unplug D0 (ESP TX -> UNO RX)**, flash, plug back. TEST_MODE=1
only for checking LDR/servo direction.

### UART protocol (one line per message, 9600 baud)

```
ESP -> UNO : E,<wifi 0=NOConf 1=Conect 2=OK>,<batt %>,<batt mV>
ESP -> UNO : M,<mode 0=auto 1=manual>,<panTarget>,<tiltTarget>
ESP -> UNO : R,<0 auto | 1 on | 2 off | 3 EMERGENCY-STOP>
ESP -> UNO : K,<panMin>,<panMax>,<tiltMin>,<tiltMax>,<solarOn mV>,<solarOff mV>,<battStop %>,<battResume %>
UNO -> ESP : U,<solar mV>,<mA>,<mW>,<mWh>,<pan>,<tilt>,<relay>,<mode>,<ldrTL>,<ldrTR>,<ldrBL>,<ldrBR>
```

- R=3 = EMERGENCY OFF: relay latches OFF, auto logic will NOT turn it back on.
  Released only when the web sends a different R (Recover button).
- If UNO loses the ESP link > 4 s, relay falls back to safe auto; ESP resends
  M/R/K every second while UNO is offline, so after an UNO reboot it gets the
  latest state back automatically.

## 2. ESP32-S3 controller

Library: **Firebase Arduino Client Library for ESP8266 and ESP32 (Mobizt)** v4.4+.
Board: **ESP32S3 Dev Module**. Serial monitor **115200**.

The Firebase defines are already filled with this project's real values
(project `solarcontroller-1cc93`) — only re-edit them if the project is recreated:

```cpp
#define FIREBASE_API_KEY   "AIzaSyCnPlrCSClwOm8tWH7EHEUiFVgKa6rEyq0"  // Project settings > General
#define FIREBASE_DB_URL    "https://solarcontroller-1cc93-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define DEVICE_EMAIL       "esp32@nhom1.com"         // device-role account in Auth
#define DEVICE_PASSWORD    "123456"                 // Firebase requires >= 6 chars
#define DASHBOARD_URL      "https://solarcontroller-1cc93.web.app/"
```

Pins already wired (do not change): UART RX=GPIO18, TX=GPIO17, battery ADC=GPIO4, baud 9600.
Flashing ESP does not require unplugging the UART wires.

### Boot flow
1. No stored WiFi **or** connect fails within 30 s -> start AP `SolarTracker-Setup`
   (pass `12345678`) with a captive-portal config page.
   User saves new WiFi -> ESP **saves to flash and reboots**, connects to new WiFi.
2. Connect OK with unchanged WiFi -> runs normally; the local web page at the
   ESP IP **auto-forwards to the Firebase dashboard after 2 s**.
3. If WiFi is lost continuously for >= 1 min, portal opens again.

Internet check: every **15 s** (`NET_CHECK_MS`) the ESP does a DNS lookup of
`pool.ntp.org`. WiFi state sent to the UNO (`E` packet) is therefore 3-valued:
**0 = NOConf** (nothing saved), **1 = Conect** (joined router but no WAN/DNS,
or not connected), **2 = OK** (internet reachable). The UNO LCD shows it as
`WiFi:NOConf | Conect | OK`.

Changing WiFi is also possible from the dashboard: it writes
`/cmd/wifiSsid`, `/cmd/wifiPass`, `/cmd/wifiTick` (integer counter, anti-replay;
the ESP compares with the last processed tick stored in flash, then saves + restarts).
Use that only while the ESP is still online on the OLD WiFi.

### 2.1 First-time WiFi setup — when the ESP cannot join your router

Symptom: the ESP never reaches `ONLINE`, it keeps broadcasting its own
`SolarTracker-Setup` access point (serial: `[WIFI] Ket loi that bai -> mo portal`
or `Chua co WiFi dang luu -> mo portal config`). Walkthrough:

1. Power the ESP (5 V, >= 500 mA) and open **Serial Monitor at 115200**.
2. On phone/PC join WiFi **`SolarTracker-Setup`**, password **`12345678`**.
3. The "Cau hinh WiFi" page opens by itself (captive portal). If it does not,
   browse to **http://192.168.4.1/setup** manually.
4. Enter your router's SSID (e.g. `hoangmanh`) and password, press
   **LUU VA KHOI DONG LAI**. The ESP stores them in flash (Preferences) and reboots.
5. Serial must show `[WIFI] Dang ket noi: <ssid>` then
   `[WIFI] OK, IP=... RSSI=...` — the dashboard status flips to `ONLINE`.

If it falls back into the portal after the reboot, the cause is one of these:

| Cause | Fix |
|---|---|
| Router (or that SSID) is 5 GHz only | **ESP32-S3 is 2.4 GHz only** — enable 2.4 GHz / use the 2.4 GHz SSID |
| Wrong password saved | retype it in the portal; a bad password always reopens the portal after 30 s |
| Hidden SSID | unhide the SSID (or broadcast it) |
| Router MAC filter / AP isolation | allow this device |
| Weak supply (USB port, MT3608 under load) | use a solid 5 V / >= 500 mA supply |
| ESP far from router, congested 2.4 GHz | move closer, set channel 1/6/11 |

Portal details: it closes after **10 min** without input (the ESP reboots and
retries whatever credentials are stored), and re-opens automatically after
**1 min** of lost WiFi.
The UART to the UNO keeps running in portal mode, so the LCD still shows battery %.

Two ways to change WiFi later:
- **ESP online** -> dashboard "Doi WiFi ESP32" panel (writes `/cmd/wifi*`, ESP reboots).
- **ESP offline** -> join `SolarTracker-Setup` again and use the portal as above.

## 3. Firebase — webController

### 3.1 Create project
(Already done for this build — project **`solarcontroller-1cc93`**. Keep these
steps only for recreating the project from scratch.)
1. console.firebase.google.com > Add project.
2. Build > **Realtime Database** > Create (locked mode).
3. Build > **Authentication** > Sign-in method > Email/Password > Enable.
4. Project settings > General > copy **Web API Key**.

### 3.2 Accounts + roles

All three accounts share the password **`123456`** (Firebase's minimum is 6
chars, so `123` is not allowed). The login page accepts a **short username**:
typing `user` or `admin` is enough — the page appends `@nhom1.com` itself.

**Status: already created in the Firebase Console (Nov 2026) with roles set** —
only recreate with the steps below if the project is reset.

| Username (login) | Email in Auth | Password | Role at /users/<uid>/role |
|---|---|---|---|
| `user` | user@nhom1.com | 123456 | user |
| `admin` | admin@nhom1.com | 123456 | admin |
| `esp32` | esp32@nhom1.com | 123456 | device (ESP firmware) |

Way A (script): Console > Service accounts > Generate new private key, save as
`seed/serviceAccountKey.json`, fix `DATABASE_URL` in seed.js, then:

```bash
cd webController/seed
npm install firebase-admin
node seed.js
```

Way B (manual): create the 3 users in Authentication, then add node
`/users/<uid>/role` in the Realtime Database (uid from the Auth Users tab).

### 3.3 Deploy

```bash
npm install -g firebase-tools
firebase login
cd webController
# firebase-config.js + .firebaserc already point at solarcontroller-1cc93
firebase deploy --only hosting,database
```

Dashboard URL: `https://<project-id>.web.app/` — same value as `DASHBOARD_URL` in the ESP sketch.

### 3.4 Rules
`database.rules.json` enforces roles server-side:
- anyone signed-in can read data;
- `user` + `admin` can write control commands (mode/angles/relay/emergency/wifi);
- only `admin` can write `cfg*` (angle limits, relay thresholds — relay breaker settings);
- only `device` (ESP) can write `/device` and `/history`;
- `/users` is read-only (roles assigned via seed/console).

## 4. Dashboard features by role

- **user**: view live telemetry, switch AUTO/MANUAL + pan/tilt sliders,
  relay auto/on/off, **EMERGENCY OFF** (latched until Recover), change WiFi.
- **admin**: everything above + CONFIG panel: pan/tilt limits, solar on/off
  thresholds (mV), battery stop/resume (%) — sent to the UNO over UART.
- Red banner appears while EMERGENCY OFF is active.

RTDB layout:

```
/device/status : "ONLINE" | "AP_SETUP" | "REBOOTING - WIFI CHANGED"
                  (best-effort — while the portal is open there is no
                   internet, so AP_SETUP usually cannot be written; the
                   dashboard does NOT use this node, it flags "ESP MAT
                   TELEMETRY" from tele staleness instead)
/device/tele   : {uno, wifi, rssi, pin_pct, pin_mv, solar_mv, solar_ma,
                  solar_mw, energy_wh, pan, tilt, relay, mode, ldr_*}
/history       : sample every 5 min (energy chart)
/cmd           : mode, pan, tilt, relay, emergency, cfg*, wifi*
```

## 5. Quick troubleshooting

| Symptom | Check |
|---|---|
| LCD shows `ESP LOST` | UART wiring, 9600 baud, D0 unplugged while flashing UNO |
| ESP keeps broadcasting `SolarTracker-Setup` | WiFi join failed — walkthrough in **§2.1** (5 GHz-only router, wrong password, hidden SSID) |
| Portal page not popping up | open http://192.168.4.1/setup manually |
| Portal saved but device never comes back ONLINE | SSID/password wrong or router is 5 GHz-only — see §2.1 |
| Dashboard shows `ESP MAT KET NOI` | no telemetry for > 5 s: ESP power, WiFi or internet down (status node may still say ONLINE — trust the tele staleness flag) |
| LCD shows `WiFi:NOConf` | no WiFi saved yet — open the portal (§2.1) |
| LCD shows `WiFi:Conect` | router joined but no internet/DNS — check WAN, DNS, router reboot |
| WiFi OK but Firebase empty | wrong API key / DB URL / device email+password |
| Relay auto never turns on | solar_mV below ON threshold, or battery full |
| Relay stays off | emergency still armed — press Recover on web |
| Dashboard login error | Auth Email/Password disabled, or no role was assigned |

## 6. Safety notes
- EMERGENCY OFF needs internet (Firebase). Without internet, the UNO keeps its
  autonomous auto-relay logic (solar voltage + battery %), so the relay can
  never get stuck ON in a fault; worst case it returns to safe auto.
