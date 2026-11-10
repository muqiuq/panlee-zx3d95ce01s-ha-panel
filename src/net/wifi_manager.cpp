#include "wifi_manager.h"

#include <WiFi.h>
#include <esp_random.h>

#include "app/config.h"

namespace net {

namespace {

constexpr uint32_t kConnectTimeoutMs = 20000;

WifiState state = WifiState::Idle;
uint32_t connectStartedMs = 0;
String apSsidValue;
String apPasswordValue;

String randomPassword() {
    static const char kChars[] = "abcdefghjkmnpqrstuvwxyz23456789";
    String pw;
    for (int i = 0; i < 10; ++i) {
        pw += kChars[esp_random() % (sizeof(kChars) - 1)];
    }
    return pw;
}

void startAccessPoint() {
    apSsidValue = "HA-Panel-" + app::deviceId();
    apPasswordValue = randomPassword();
    // AP+STA keeps WiFi scanning available for the setup page.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSsidValue.c_str(), apPasswordValue.c_str());
    state = WifiState::AccessPoint;
    log_i("AP %s started, IP %s", apSsidValue.c_str(), WiFi.softAPIP().toString().c_str());
}

void startStation() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(app::config.wifi.ssid.c_str(), app::config.wifi.password.c_str());
    connectStartedMs = millis();
    state = WifiState::Connecting;
}

}  // namespace

void wifiBegin() {
    WiFi.persistent(false);
    WiFi.setHostname(app::config.wifi.hostname.c_str());
    if (app::config.wifi.ssid.isEmpty()) {
        startAccessPoint();
    } else {
        startStation();
    }
}

void wifiLoop() {
    const bool connected = WiFi.status() == WL_CONNECTED;
    switch (state) {
        case WifiState::Connecting:
            if (connected) {
                state = WifiState::Connected;
                log_i("WiFi connected, IP %s", WiFi.localIP().toString().c_str());
            } else if (millis() - connectStartedMs > kConnectTimeoutMs) {
                startAccessPoint();
            }
            break;
        case WifiState::AccessPoint:
            // Keep retrying the station in the background; drop the AP once it connects.
            if (connected) {
                WiFi.softAPdisconnect(true);
                WiFi.mode(WIFI_STA);
                state = WifiState::Connected;
            }
            break;
        case WifiState::Connected:
            if (!connected) {
                connectStartedMs = millis();
                state = WifiState::Connecting;
            }
            break;
        case WifiState::Idle:
            break;
    }
}

WifiState wifiState() {
    return state;
}

const char* wifiStateName() {
    switch (state) {
        case WifiState::Connecting: return "Connecting";
        case WifiState::Connected: return "Connected";
        case WifiState::AccessPoint: return "Setup AP";
        default: return "Idle";
    }
}

const String& apSsid() {
    return apSsidValue;
}

const String& apPassword() {
    return apPasswordValue;
}

}  // namespace net
