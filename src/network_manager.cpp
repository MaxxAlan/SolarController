#include "network_manager.h"
#include "esp_log.h"

static const char* NET_TAG = "NET";

// Chỉ báo LED RGB onboard (WS2812 GPIO48)
static void rgbStatus(uint8_t r, uint8_t g, uint8_t b) {
    rgbLedWrite(RGB_BUILTIN, r, g, b);
}

// HTML UI Dashboard được nhúng trực tiếp vào Flash ESP32-S3
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SolarController Dashboard</title>
    <style>
        :root {
            --bg-dark: #0f172a;
            --card-bg: rgba(30, 41, 59, 0.7);
            --border: rgba(255, 255, 255, 0.1);
            --primary: #f59e0b;
            --primary-glow: rgba(245, 158, 11, 0.3);
            --accent: #38bdf8;
            --success: #10b981;
            --danger: #ef4444;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
        body {
            background: radial-gradient(circle at top right, #1e293b, var(--bg-dark));
            color: var(--text-main);
            min-height: 100vh;
            padding: 20px;
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        .container { width: 100%; max-width: 900px; }
        header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 16px 20px;
            background: var(--card-bg);
            backdrop-filter: blur(12px);
            border-radius: 16px;
            border: 1px solid var(--border);
            margin-bottom: 20px;
        }
        .logo-area { display: flex; align-items: center; gap: 12px; }
        .logo-icon { font-size: 28px; filter: drop-shadow(0 0 8px var(--primary)); }
        .title h1 { font-size: 1.3rem; font-weight: 700; letter-spacing: -0.5px; }
        .title p { font-size: 0.8rem; color: var(--text-muted); }
        .badge {
            padding: 6px 14px;
            border-radius: 20px;
            font-size: 0.8rem;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            display: inline-flex;
            align-items: center;
            gap: 6px;
        }
        .badge-online { background: rgba(16, 185, 129, 0.2); color: var(--success); border: 1px solid rgba(16, 185, 129, 0.4); }
        .badge-offline { background: rgba(239, 68, 68, 0.2); color: var(--danger); border: 1px solid rgba(239, 68, 68, 0.4); }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
            gap: 16px;
            margin-bottom: 20px;
        }
        .card {
            background: var(--card-bg);
            backdrop-filter: blur(12px);
            border-radius: 16px;
            border: 1px solid var(--border);
            padding: 20px;
            display: flex;
            flex-direction: column;
        }
        .card h2 {
            font-size: 0.95rem;
            color: var(--text-muted);
            margin-bottom: 14px;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }
        .val-display { font-size: 2.2rem; font-weight: 800; color: var(--text-main); margin-bottom: 4px; }
        .unit { font-size: 1rem; font-weight: 500; color: var(--text-muted); }
        .ldr-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-top: 8px;
        }
        .ldr-box {
            background: rgba(15, 23, 42, 0.6);
            border: 1px solid var(--border);
            border-radius: 10px;
            padding: 10px;
            text-align: center;
        }
        .ldr-box span { font-size: 0.75rem; color: var(--text-muted); display: block; }
        .ldr-box strong { font-size: 1.1rem; color: var(--primary); }
        .progress-bar {
            width: 100%;
            height: 10px;
            background: rgba(255, 255, 255, 0.1);
            border-radius: 5px;
            overflow: hidden;
            margin-top: 8px;
        }
        .progress-fill {
            height: 100%;
            background: linear-gradient(90deg, #10b981, #f59e0b);
            width: 0%;
            transition: width 0.3s ease;
        }
        .controls {
            background: var(--card-bg);
            backdrop-filter: blur(12px);
            border-radius: 16px;
            border: 1px solid var(--border);
            padding: 24px;
            margin-bottom: 20px;
        }
        .btn-group { display: flex; gap: 10px; margin-bottom: 20px; }
        .btn {
            flex: 1;
            padding: 12px;
            border-radius: 10px;
            border: 1px solid var(--border);
            background: rgba(15, 23, 42, 0.6);
            color: var(--text-main);
            font-size: 0.9rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s;
        }
        .btn.active {
            background: var(--primary);
            color: #000;
            border-color: var(--primary);
            box-shadow: 0 0 15px var(--primary-glow);
        }
        .slider-wrap { margin-top: 15px; }
        .slider-header { display: flex; justify-content: space-between; margin-bottom: 8px; font-size: 0.9rem; }
        input[type=range] {
            width: 100%;
            height: 8px;
            border-radius: 5px;
            background: rgba(255, 255, 255, 0.15);
            outline: none;
            -webkit-appearance: none;
        }
        input[type=range]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 22px;
            height: 22px;
            border-radius: 50%;
            background: var(--primary);
            cursor: pointer;
            box-shadow: 0 0 10px var(--primary-glow);
        }
        .preset-btns { display: flex; gap: 8px; margin-top: 15px; }
        .preset-btn {
            flex: 1;
            padding: 8px;
            font-size: 0.8rem;
            border-radius: 8px;
            background: rgba(255, 255, 255, 0.05);
            border: 1px solid var(--border);
            color: var(--text-muted);
            cursor: pointer;
        }
        .preset-btn:hover { background: rgba(255, 255, 255, 0.1); color: #fff; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <div class="logo-area">
                <div class="logo-icon">☀️</div>
                <div class="title">
                    <h1>SolarController IoT</h1>
                    <p>ESP32-S3 Smart Solar Tracker</p>
                </div>
            </div>
            <div id="statusBadge" class="badge badge-online">ONLINE</div>
        </header>

        <div class="grid">
            <div class="card">
                <h2>Góc Xoay Servo <span id="modeBadge" style="color: var(--accent); font-weight:700;">AUTO</span></h2>
                <div class="val-display"><span id="curAngle">--</span><span class="unit">°</span></div>
                <p style="font-size: 0.8rem; color: var(--text-muted);">Mục tiêu: <span id="tgtAngle">--</span>°</p>
                <div class="progress-bar">
                    <div id="angleFill" class="progress-fill"></div>
                </div>
            </div>

            <div class="card">
                <h2>Pin & Điện Áp <span id="socText">--%</span></h2>
                <div class="val-display"><span id="batVolts">--</span><span class="unit">V</span></div>
                <p style="font-size: 0.8rem; color: var(--text-muted);">Trạng thái: <span id="batStatus">Bình thường</span></p>
                <div class="progress-bar">
                    <div id="socFill" class="progress-fill"></div>
                </div>
            </div>

            <div class="card">
                <h2>Cảm Biến Quang Học (4x LDR)</h2>
                <div class="ldr-grid">
                    <div class="ldr-box"><span>Top-Left</span><strong id="ldrTL">0</strong></div>
                    <div class="ldr-box"><span>Top-Right</span><strong id="ldrTR">0</strong></div>
                    <div class="ldr-box"><span>Bot-Left</span><strong id="ldrBL">0</strong></div>
                    <div class="ldr-box"><span>Bot-Right</span><strong id="ldrBR">0</strong></div>
                </div>
                <p style="font-size:0.75rem; color:var(--text-muted); margin-top:8px; text-align:center;">
                    Chênh lệch L-R: <strong id="ldrDiff" style="color:#fff;">0</strong>
                </p>
            </div>
        </div>

        <div class="controls">
            <h2 style="font-size: 1.1rem; margin-bottom: 14px;">Bảng Điều Khiển</h2>
            <div class="btn-group">
                <button id="btnAuto" class="btn active" onclick="setMode('AUTO')">Chế độ Tự Động (Auto)</button>
                <button id="btnManual" class="btn" onclick="setMode('MANUAL')">Chế độ Thủ Công (Manual)</button>
            </div>

            <div class="slider-wrap" id="sliderGroup" style="opacity: 0.4; pointer-events: none;">
                <div class="slider-header">
                    <span>Góc xoay thủ công:</span>
                    <strong id="sliderVal" style="color: var(--primary);">90°</strong>
                </div>
                <input type="range" id="angleSlider" min="10" max="170" value="90" oninput="onSliderInput(this.value)" onchange="sendManualAngle(this.value)">
                <div class="preset-btns">
                    <button class="preset-btn" onclick="setPreset(20)">Hướng Đông (20°)</button>
                    <button class="preset-btn" onclick="setPreset(90)">Chính Ngọ (90°)</button>
                    <button class="preset-btn" onclick="setPreset(160)">Hướng Tây (160°)</button>
                </div>
            </div>
        </div>
    </div>

    <script>
        let currentMode = "AUTO";

        function updateUI(data) {
            document.getElementById('curAngle').innerText = data.current_angle;
            document.getElementById('tgtAngle').innerText = data.target_angle;
            document.getElementById('angleFill').style.width = ((data.current_angle / 180) * 100) + '%';

            document.getElementById('batVolts').innerText = data.battery_voltage.toFixed(2);
            document.getElementById('socText').innerText = data.battery_soc + '%';
            document.getElementById('socFill').style.width = data.battery_soc + '%';

            document.getElementById('ldrTL').innerText = data.ldr.tl;
            document.getElementById('ldrTR').innerText = data.ldr.tr;
            document.getElementById('ldrBL').innerText = data.ldr.bl;
            document.getElementById('ldrBR').innerText = data.ldr.br;
            document.getElementById('ldrDiff').innerText = data.ldr.diff;

            document.getElementById('modeBadge').innerText = data.mode;

            if (data.mode !== currentMode) {
                currentMode = data.mode;
                applyModeState(currentMode);
            }
        }

        function applyModeState(mode) {
            const btnAuto = document.getElementById('btnAuto');
            const btnManual = document.getElementById('btnManual');
            const sliderGroup = document.getElementById('sliderGroup');

            if (mode === "AUTO" || mode === "NIGHT") {
                btnAuto.classList.add('active');
                btnManual.classList.remove('active');
                sliderGroup.style.opacity = '0.4';
                sliderGroup.style.pointerEvents = 'none';
            } else {
                btnAuto.classList.remove('active');
                btnManual.classList.add('active');
                sliderGroup.style.opacity = '1';
                sliderGroup.style.pointerEvents = 'auto';
            }
        }

        function setMode(mode) {
            fetch('/api/command', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ mode: mode })
            }).then(() => applyModeState(mode));
        }

        function onSliderInput(val) {
            document.getElementById('sliderVal').innerText = val + '°';
        }

        function sendManualAngle(val) {
            fetch('/api/command', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ mode: "MANUAL", angle: parseInt(val) })
            });
        }

        function setPreset(val) {
            document.getElementById('angleSlider').value = val;
            onSliderInput(val);
            sendManualAngle(val);
        }

        function fetchTelemetry() {
            fetch('/api/telemetry')
                .then(r => r.json())
                .then(data => updateUI(data))
                .catch(err => {
                    document.getElementById('statusBadge').innerText = "DISCONNECTED";
                    document.getElementById('statusBadge').className = "badge badge-offline";
                });
        }

        setInterval(fetchTelemetry, 1000);
        fetchTelemetry();
    </script>
</body>
</html>
)rawliteral";

SolarNetworkManager::SolarNetworkManager(SolarTracker& tracker, BatteryMonitor& battery)
    : tracker(tracker),
      battery(battery),
      onlineState(false),
      lastWiFiCheckMs(0),
      lastTelemetrySendMs(0),
      lastSentWasValid(true),
      wifiSwitchInProgress(false),
      wifiSwitchStartMs(0),
      server(LOCAL_WEB_PORT)
#if ENABLE_FIREBASE
      , firebaseReady(false)
#endif
{
}

void SolarNetworkManager::begin() {
    rgbStatus(200, 80, 0);  // cam: khởi động mạng
    // Đọc cấu hình Wi-Fi đã lưu trong NVS (ưu tiên) hoặc mặc định ở config.h
    // readOnly=false để tự tạo namespace khi chưa tồn tại
    prefs.begin("solarcfg", false);
    activeSsid = prefs.getString("ssid", WIFI_SSID);
    activePass = prefs.getString("pass", WIFI_PASSWORD);
    prefs.end();
    rgbStatus(0, 0, 150);  // xanh dương: đang bật WiFi

    Serial.println("\n[NETWORK] Connecting to Wi-Fi...");
    Serial.printf("[NETWORK] SSID: %s\n", activeSsid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(activeSsid.c_str(), activePass.c_str());

    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs < WIFI_CONNECT_TIMEOUT_MS)) {
        delay(250);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        onlineState = true;
        rgbStatus(0, 150, 0);  // xanh lá: WiFi OK
        String ip = WiFi.localIP().toString();
        Serial.println("\n[NETWORK] Wi-Fi Connected!");
        Serial.printf("[NETWORK] IP Address: %s\n", ip.c_str());
        ESP_LOGI(NET_TAG, "WiFi OK ssid=%s ip=%s rssi=%d",
                 activeSsid.c_str(), ip.c_str(), WiFi.RSSI());
    } else {
        onlineState = false;
        rgbStatus(150, 0, 0);  // đỏ: WiFi fail
        Serial.println("\n[NETWORK] Wi-Fi Connection Timeout -> Entering OFFLINE FAILSAFE Mode");
        ESP_LOGE(NET_TAG, "WiFi FAIL ssid=%s status=%d", activeSsid.c_str(), (int)WiFi.status());
    }

    // Luôn khởi tạo Web server & Firebase (sẽ tự đồng bộ khi có mạng)
#if ENABLE_LOCAL_WEB_SERVER
    setupLocalWebServer();
#endif

#if ENABLE_FIREBASE
    initFirebase();
#endif
}

// Nhận cấu hình Wi-Fi mới từ Cloud: giữ pending trong RAM, thử kết nối 20s,
// chỉ lưu NVS khi thành công, nếu thất bại tự khôi phục cấu hình cũ
void SolarNetworkManager::startWifiSwitch(const String& ssid, const String& pass) {
    Serial.printf("[NETWORK] Wi-Fi switch requested -> SSID: %s\n", ssid.c_str());

    prevSsid = activeSsid;
    prevPass = activePass;
    activeSsid = ssid;
    activePass = pass;

    wifiSwitchInProgress = true;
    wifiSwitchStartMs = millis();
    onlineState = false;

    WiFi.disconnect();
    WiFi.begin(activeSsid.c_str(), activePass.c_str());
}

void SolarNetworkManager::updateWifiSwitch() {
    if (!wifiSwitchInProgress) return;

    if (WiFi.status() == WL_CONNECTED) {
        wifiSwitchInProgress = false;
        onlineState = true;
        prefs.begin("solarcfg", false);
        prefs.putString("ssid", activeSsid);
        prefs.putString("pass", activePass);
        prefs.end();
        Serial.printf("[NETWORK] Wi-Fi switch OK! New IP: %s\n", WiFi.localIP().toString().c_str());
        return;
    }

    if (millis() - wifiSwitchStartMs > 20000) {
        Serial.println("[NETWORK] Wi-Fi switch TIMEOUT -> reverting to previous credentials");
        wifiSwitchInProgress = false;
        activeSsid = prevSsid;
        activePass = prevPass;

        WiFi.disconnect();
        WiFi.begin(activeSsid.c_str(), activePass.c_str());
    }
}

void SolarNetworkManager::setupLocalWebServer() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/api/telemetry", HTTP_GET, [this]() { handleApiTelemetry(); });
    server.on("/api/command", HTTP_POST, [this]() { handleApiCommand(); });

    server.begin();
    Serial.printf("[WEB] Local Dashboard available at http://%s/\n", WiFi.localIP().toString().c_str());
}

void SolarNetworkManager::handleRoot() {
    server.send_P(200, "text/html", DASHBOARD_HTML);
}

void SolarNetworkManager::handleApiTelemetry() {
    JsonDocument doc;
    LDRReading ldr = tracker.getLDRData();

    doc["current_angle"] = tracker.getCurrentAngle();
    doc["target_angle"] = tracker.getTargetAngle();
    doc["mode"] = tracker.getModeString();
    doc["battery_voltage"] = battery.getVoltage();
    doc["battery_soc"] = battery.getSOC();
    doc["is_fault"] = tracker.isFault();
    doc["fault_reason"] = tracker.getFaultReason();

    JsonObject ldrObj = doc["ldr"].to<JsonObject>();
    ldrObj["tl"] = ldr.topLeft;
    ldrObj["tr"] = ldr.topRight;
    ldrObj["bl"] = ldr.bottomLeft;
    ldrObj["br"] = ldr.bottomRight;
    ldrObj["diff"] = ldr.diff;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
}

void SolarNetworkManager::handleApiCommand() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Bad Request: No body");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
        server.send(400, "text/plain", "Bad Request: Invalid JSON");
        return;
    }

    if (doc.containsKey("mode")) {
        const char* modeStr = doc["mode"];
        if (strcmp(modeStr, "AUTO") == 0) {
            tracker.setMode(MODE_AUTO);
        } else if (strcmp(modeStr, "MANUAL") == 0) {
            tracker.setMode(MODE_MANUAL);
        }
    }

    if (doc.containsKey("angle")) {
        int angle = doc["angle"];
        tracker.setManualAngle(angle);
    }

    server.send(200, "application/json", "{\"status\":\"ok\"}");
}

#if ENABLE_FIREBASE
void SolarNetworkManager::initFirebase() {
    Serial.println("[FIREBASE] Initializing Firebase connection...");
    fbConfig.host = FIREBASE_HOST;
    fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;

    Firebase.begin(&fbConfig, &auth);
    Firebase.reconnectWiFi(true);
    firebaseReady = true;
    Serial.println("[FIREBASE] Initialized.");
    ESP_LOGI(NET_TAG, "Firebase init OK host=%s", FIREBASE_HOST);
}

void SolarNetworkManager::sendFirebaseTelemetry() {
    if (!firebaseReady) return;

    LDRReading ldr = tracker.getLDRData();

    // Chỉ gửi dữ liệu THẬT. Sensor chưa đọc được hợp lệ -> xóa node (DB = null)
    bool ldrValid = !(ldr.topLeft == 0 && ldr.topRight == 0 &&
                      ldr.bottomLeft == 0 && ldr.bottomRight == 0) &&
                    !(ldr.topLeft >= 4095 && ldr.topRight >= 4095 &&
                      ldr.bottomLeft >= 4095 && ldr.bottomRight >= 4095);
    bool batValid = battery.getVoltage() > 1.0f && battery.getVoltage() < 5.0f;

    static int validLogCount = 0;
    if (validLogCount++ % 4 == 0) {
        ESP_LOGE(NET_TAG, "TEL ldr=[%d,%d,%d,%d] v=%.2f soc=%d valid=%d/%d",
                 ldr.topLeft, ldr.topRight, ldr.bottomLeft, ldr.bottomRight,
                 (double)battery.getVoltage(), battery.getSOC(),
                 (int)ldrValid, (int)batValid);
    }

    if (!ldrValid || !batValid) {
        if (lastSentWasValid) {
            Firebase.RTDB.deleteNode(&fbdo, "/solar_controller/telemetry");
            lastSentWasValid = false;
        }
        return;
    }

    FirebaseJson json;
    json.set("angle", tracker.getCurrentAngle());
    json.set("mode", tracker.getModeString());
    json.set("voltage", battery.getVoltage());
    json.set("soc", battery.getSOC());
    json.set("ldr_tl", ldr.topLeft);
    json.set("ldr_tr", ldr.topRight);
    json.set("ldr_bl", ldr.bottomLeft);
    json.set("ldr_br", ldr.bottomRight);
    json.set("diff", ldr.diff);
    json.set("wifi_ssid", activeSsid);
    json.set("wifi_ip", WiFi.localIP().toString());

    if (!Firebase.RTDB.setJSON(&fbdo, "/solar_controller/telemetry", &json)) {
        ESP_LOGE(NET_TAG, "TEL WRITE FAIL code=%d reason=%s",
                 fbdo.errorCode(), fbdo.errorReason().c_str());
        return;
    }
    lastSentWasValid = true;
}

void SolarNetworkManager::readFirebaseCommands() {
    if (!firebaseReady) return;

    if (Firebase.RTDB.getJSON(&fbdo, "/solar_controller/command")) {
        FirebaseJson &json = fbdo.jsonObject();
        FirebaseJsonData jsonData;

        if (json.get(jsonData, "mode")) {
            String m = jsonData.stringValue;
            if (m == "AUTO") tracker.setMode(MODE_AUTO);
            else if (m == "MANUAL") tracker.setMode(MODE_MANUAL);
        }

        if (json.get(jsonData, "angle")) {
            tracker.setManualAngle(jsonData.intValue);
        }

        // Cấu hình Wi-Fi từ Dashboard: {wifi_ssid, wifi_pass}
        if (json.get(jsonData, "wifi_ssid")) {
            String newSsid = jsonData.stringValue;
            String newPass = "";
            json.get(jsonData, "wifi_pass");
            newPass = jsonData.stringValue;

            if (newSsid.length() > 0 && newSsid != activeSsid && !wifiSwitchInProgress) {
                // Xóa lệnh TRƯỚC khi ngắt Wi-Fi để mật khẩu không nằm lại trong Cloud
                Firebase.RTDB.deleteNode(&fbdo, "/solar_controller/command");
                startWifiSwitch(newSsid, newPass);
            }
        }
    }
}
#endif

void SolarNetworkManager::checkWiFiConnection() {
    unsigned long now = millis();
    if (now - lastWiFiCheckMs < 10000) return;
    lastWiFiCheckMs = now;

    if (WiFi.status() == WL_CONNECTED) {
        if (!onlineState) {
            onlineState = true;
            Serial.println("[NETWORK] Wi-Fi Reconnected!");
        }
    } else {
        if (onlineState) {
            onlineState = false;
            Serial.println("[NETWORK] Wi-Fi Disconnected -> Switched to OFFLINE mode");
        }
    }
}

void SolarNetworkManager::update() {
    static bool stgUpdate = false;
    updateWifiSwitch();
    checkWiFiConnection();

#if ENABLE_LOCAL_WEB_SERVER
    if (onlineState) {
        server.handleClient();
    }
#endif

    unsigned long now = millis();
    if (onlineState && (now - lastTelemetrySendMs >= TELEMETRY_SEND_INTERVAL_MS)) {
        lastTelemetrySendMs = now;
#if ENABLE_FIREBASE
        static bool stg = false;
        sendFirebaseTelemetry();
        readFirebaseCommands();
#endif
    }

    static unsigned long lastHbMs = 0;
    if (now - lastHbMs >= 10000) {
        lastHbMs = now;
        ESP_LOGI(NET_TAG, "HB online=%d wifi=%d rssi=%d angle=%.1f mode=%s",
                 onlineState, WiFi.status() == WL_CONNECTED, WiFi.RSSI(),
                 (double)tracker.getCurrentAngle(), tracker.getModeString());
    }
}

