#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "config.h"
#include "tracker.h"
#include "battery_monitor.h"

#if ENABLE_FIREBASE
#include <Firebase_ESP_Client.h>
#endif

class SolarNetworkManager {
public:
    SolarNetworkManager(SolarTracker& tracker, BatteryMonitor& battery);
    void begin();
    void update();

    bool isOnline() const { return onlineState; }
    String getIPAddress() const { return WiFi.localIP().toString(); }

private:
    SolarTracker& tracker;
    BatteryMonitor& battery;

    bool onlineState;
    unsigned long lastWiFiCheckMs;
    unsigned long lastTelemetrySendMs;

    // Cấu hình Wi-Fi động (nhận từ Cloud, lưu vào NVS)
    Preferences prefs;
    String activeSsid;
    String activePass;
    bool wifiSwitchInProgress;
    unsigned long wifiSwitchStartMs;
    String prevSsid;
    String prevPass;

    void startWifiSwitch(const String& ssid, const String& pass);
    void updateWifiSwitch();

    WebServer server;

#if ENABLE_FIREBASE
    FirebaseData fbdo;
    FirebaseAuth auth;
    FirebaseConfig fbConfig;
    bool firebaseReady;
    void initFirebase();
    void sendFirebaseTelemetry();
    void readFirebaseCommands();
#endif

    void setupLocalWebServer();
    void handleRoot();
    void handleApiTelemetry();
    void handleApiCommand();
    void checkWiFiConnection();
};
