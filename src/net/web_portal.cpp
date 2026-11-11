#include "web_portal.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

#include "app/app.h"
#include "app/config.h"
#include "app/power.h"
#include "hw/climate_sensor.h"
#include "mqtt_service.h"
#include "ui/background.h"
#include "ui/message.h"
#include "ui/page_manager.h"
#include "ui/ui.h"
#include "version.h"
#include "web/index_html.h"
#include "wifi_manager.h"

namespace net {

namespace {

constexpr const char* kBackgroundTmp = "/bg.tmp";

WebServer server(80);
DNSServer dns;
bool dnsRunning = false;

bool uploadAuthorized = false;
String uploadError;
File uploadFile;
size_t uploadSize = 0;

bool authorized() {
    const String& pw = app::config.web.password;
    if (pw.isEmpty() || server.authenticate("admin", pw.c_str())) {
        return true;
    }
    server.requestAuthentication(BASIC_AUTH, "HA Panel");
    return false;
}

void sendJson(int code, const JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    server.sendHeader("Cache-Control", "no-store");
    server.send(code, "application/json", out);
}

void sendOk() {
    server.send(200, "application/json", "{\"ok\":true}");
}

void sendError(int code, const String& message) {
    JsonDocument doc;
    doc["error"] = message;
    sendJson(code, doc);
}

void handleIndex() {
    if (!authorized()) {
        return;
    }
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html", reinterpret_cast<const char*>(INDEX_HTML_GZ), INDEX_HTML_GZ_LEN);
}

void handleNotFound() {
    // Captive portal: send every unknown request to the UI while the setup AP is active.
    if (wifiState() == WifiState::AccessPoint && !server.uri().startsWith("/api/")) {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
        server.send(302, "text/plain", "");
        return;
    }
    sendError(404, "not found");
}

const char* resetReason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "power on";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt watchdog";
        case ESP_RST_TASK_WDT: return "task watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_BROWNOUT: return "brownout";
        default: return "unknown";
    }
}

void handleStatus() {
    if (!authorized()) {
        return;
    }
    JsonDocument doc;
    JsonObject dev = doc["device"].to<JsonObject>();
    dev["id"] = app::deviceId();
    dev["hostname"] = app::config.wifi.hostname;
    dev["firmware"] = FIRMWARE_VERSION;
    dev["build"] = __DATE__ " " __TIME__;
    dev["uptime"] = millis() / 1000;
    dev["heapFree"] = ESP.getFreeHeap();
    dev["heapTotal"] = ESP.getHeapSize();
    dev["psramFree"] = ESP.getFreePsram();
    dev["psramTotal"] = ESP.getPsramSize();
    dev["flash"] = ESP.getFlashChipSize();
    dev["sketchSize"] = ESP.getSketchSize();
    dev["sketchFree"] = ESP.getFreeSketchSpace();
    dev["fsUsed"] = LittleFS.usedBytes();
    dev["fsTotal"] = LittleFS.totalBytes();
    dev["chip"] = ESP.getChipModel();
    dev["chipRevision"] = ESP.getChipRevision();
    dev["cores"] = ESP.getChipCores();
    dev["cpuMhz"] = ESP.getCpuFreqMHz();
    dev["resetReason"] = resetReason();
    dev["watchdogSeconds"] = app::kWatchdogSeconds;
    time_t now = time(nullptr);
    if (now > 1600000000) {
        char buf[32];
        struct tm t;
        localtime_r(&now, &t);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
        dev["time"] = buf;
    }

    JsonObject w = doc["wifi"].to<JsonObject>();
    w["state"] = wifiStateName();
    w["ssid"] = WiFi.SSID();
    w["rssi"] = WiFi.RSSI();
    w["channel"] = WiFi.channel();
    w["ip"] = WiFi.localIP().toString();
    w["gateway"] = WiFi.gatewayIP().toString();
    w["subnet"] = WiFi.subnetMask().toString();
    w["dns"] = WiFi.dnsIP().toString();
    w["mac"] = WiFi.macAddress();
    w["bssid"] = WiFi.BSSIDstr();
    w["rebootOnLoss"] = app::config.wifi.rebootOnLoss;
    w["rebootAfterMinutes"] = app::config.wifi.rebootAfterMinutes;
    w["offlineSeconds"] = wifiState() == WifiState::Connected ? 0 : app::wifiOfflineSeconds();
    if (wifiState() == WifiState::AccessPoint) {
        w["apSsid"] = apSsid();
        w["apIp"] = WiFi.softAPIP().toString();
    }

    JsonObject m = doc["mqtt"].to<JsonObject>();
    m["state"] = mqttStateName();
    m["connected"] = mqttConnected();
    m["host"] = app::config.mqtt.host;
    m["port"] = app::config.mqtt.port;
    m["baseTopic"] = app::config.mqtt.baseTopic;

    JsonObject d = doc["display"].to<JsonObject>();
    d["state"] = app::powerStateName();
    d["page"] = ui::currentPage() + 1;
    d["pages"] = ui::pageCount();
    d["background"] = ui::backgroundImage() != nullptr;

    JsonObject sensor = doc["sensor"].to<JsonObject>();
    sensor["available"] = hw::climateAvailable();
    if (!isnan(app::panelTemperature())) {
        sensor["temperature"] = roundf(app::panelTemperature() * 10) / 10;
        sensor["humidity"] = roundf(app::panelHumidity() * 10) / 10;
        sensor["rawTemperature"] = roundf(hw::climateTemperature() * 10) / 10;
        sensor["rawHumidity"] = roundf(hw::climateHumidity() * 10) / 10;
    }

    const ui::MessageInfo& msg = ui::lastMessage();
    if (!msg.time.isEmpty()) {
        JsonObject mo = doc["message"].to<JsonObject>();
        mo["id"] = msg.id;
        mo["title"] = msg.title;
        mo["text"] = msg.text;
        mo["level"] = msg.level;
        mo["buttons"] = msg.buttons;
        mo["time"] = msg.time;
        mo["open"] = msg.open;
        mo["answer"] = msg.answer;
    }

    JsonObject a = doc["author"].to<JsonObject>();
    a["name"] = "Philipp Albrecht";
    a["email"] = "philipp@uisa.ch";
    a["web"] = "www.uisa.ch";
    sendJson(200, doc);
}

void handleGetConfig() {
    if (!authorized()) {
        return;
    }
    JsonDocument doc;
    app::configToJson(app::config, doc, true);
    sendJson(200, doc);
}

void handlePutConfig() {
    if (!authorized()) {
        return;
    }
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        sendError(400, String("invalid JSON: ") + err.c_str());
        return;
    }
    const app::Config previous = app::config;
    app::configFromJson(doc.as<JsonVariantConst>(), app::config);
    if (!app::configSave()) {
        app::config = previous;
        sendError(500, "saving failed");
        return;
    }
    sendOk();
    app::applyConfig(previous);
}

void handleWifiScan() {
    if (!authorized()) {
        return;
    }
    const int n = WiFi.scanNetworks(false, false);
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < n; ++i) {
        JsonObject o = arr.add<JsonObject>();
        o["ssid"] = WiFi.SSID(i);
        o["rssi"] = WiFi.RSSI(i);
        o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    sendJson(200, doc);
}

void handleReboot() {
    if (!authorized()) {
        return;
    }
    sendOk();
    app::scheduleRestart(500);
}

void handleFactoryReset() {
    if (!authorized()) {
        return;
    }
    app::configFactoryReset();
    LittleFS.remove(ui::kBackgroundPath);
    sendOk();
    app::scheduleRestart(500);
}

void handlePanelAction() {
    if (!authorized()) {
        return;
    }
    const String action = server.arg("action");
    if (action == "wake") {
        app::powerWake();
    } else if (action == "sleep") {
        app::powerSleep();
    } else if (action == "page") {
        app::powerWake();
        ui::showPage(static_cast<size_t>(max(0L, server.arg("page").toInt() - 1)));
    } else {
        sendError(400, "unknown action");
        return;
    }
    sendOk();
}

void handleMessage() {
    if (!authorized()) {
        return;
    }
    String error;
    if (!ui::messageShow(server.arg("plain"), error)) {
        sendError(400, error);
        return;
    }
    sendOk();
}

void handleGetBackground() {
    if (!authorized()) {
        return;
    }
    File f = LittleFS.open(ui::kBackgroundPath, "r");
    if (!f) {
        sendError(404, "no background");
        return;
    }
    server.sendHeader("Cache-Control", "no-store");
    server.streamFile(f, "application/octet-stream");
    f.close();
}

void handleDeleteBackground() {
    if (!authorized()) {
        return;
    }
    LittleFS.remove(ui::kBackgroundPath);
    ui::rebuild(true);
    sendOk();
}

void handleBackgroundUpload() {
    HTTPUpload& up = server.upload();
    switch (up.status) {
        case UPLOAD_FILE_START: {
            const String& pw = app::config.web.password;
            uploadAuthorized = pw.isEmpty() || server.authenticate("admin", pw.c_str());
            uploadError = "";
            uploadSize = 0;
            if (uploadAuthorized) {
                app::powerBlank(true);
                uploadFile = LittleFS.open(kBackgroundTmp, "w");
                if (!uploadFile) {
                    uploadError = "cannot open file";
                }
            }
            break;
        }
        case UPLOAD_FILE_WRITE:
            if (uploadAuthorized && uploadFile && uploadError.isEmpty()) {
                uploadSize += up.currentSize;
                if (uploadSize > ui::kBackgroundBytes) {
                    uploadError = "image too large";
                } else if (uploadFile.write(up.buf, up.currentSize) != up.currentSize) {
                    uploadError = "write failed (filesystem full?)";
                }
            }
            break;
        case UPLOAD_FILE_END:
        case UPLOAD_FILE_ABORTED:
            if (uploadFile) {
                uploadFile.close();
            }
            if (up.status == UPLOAD_FILE_ABORTED) {
                uploadError = "upload aborted";
                app::powerBlank(false);
            }
            break;
    }
}

void handleBackgroundDone() {
    if (!uploadAuthorized) {
        server.requestAuthentication(BASIC_AUTH, "HA Panel");
        return;
    }
    if (uploadError.isEmpty() && uploadSize != ui::kBackgroundBytes) {
        uploadError = "expected 480x480 RGB565 (" + String(ui::kBackgroundBytes) + " bytes)";
    }
    if (!uploadError.isEmpty()) {
        LittleFS.remove(kBackgroundTmp);
        app::powerBlank(false);
        sendError(400, uploadError);
        return;
    }
    LittleFS.remove(ui::kBackgroundPath);
    LittleFS.rename(kBackgroundTmp, ui::kBackgroundPath);
    ui::rebuild(true);
    app::powerBlank(false);
    sendOk();
}

void handleOtaUpload() {
    HTTPUpload& up = server.upload();
    switch (up.status) {
        case UPLOAD_FILE_START: {
            const String& pw = app::config.web.password;
            uploadAuthorized = pw.isEmpty() || server.authenticate("admin", pw.c_str());
            uploadError = "";
            if (uploadAuthorized) {
                app::powerBlank(true);
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                    uploadError = Update.errorString();
                }
            }
            break;
        }
        case UPLOAD_FILE_WRITE:
            if (uploadAuthorized && uploadError.isEmpty() &&
                Update.write(up.buf, up.currentSize) != up.currentSize) {
                uploadError = Update.errorString();
            }
            break;
        case UPLOAD_FILE_END:
            if (uploadAuthorized && uploadError.isEmpty() && !Update.end(true)) {
                uploadError = Update.errorString();
            }
            break;
        case UPLOAD_FILE_ABORTED:
            Update.abort();
            uploadError = "upload aborted";
            app::powerBlank(false);
            break;
    }
}

void handleOtaDone() {
    if (!uploadAuthorized) {
        server.requestAuthentication(BASIC_AUTH, "HA Panel");
        return;
    }
    if (!uploadError.isEmpty()) {
        Update.abort();
        app::powerBlank(false);
        sendError(400, uploadError);
        return;
    }
    // Backlight stays off until the reboot.
    sendOk();
    app::scheduleRestart(1000);
}

}  // namespace

void webBegin() {
    server.on("/", HTTP_GET, handleIndex);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/config", HTTP_GET, handleGetConfig);
    server.on("/api/config", HTTP_PUT, handlePutConfig);
    server.on("/api/config", HTTP_POST, handlePutConfig);
    server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
    server.on("/api/reboot", HTTP_POST, handleReboot);
    server.on("/api/factory-reset", HTTP_POST, handleFactoryReset);
    server.on("/api/panel", HTTP_POST, handlePanelAction);
    server.on("/api/message", HTTP_POST, handleMessage);
    server.on("/api/background", HTTP_GET, handleGetBackground);
    server.on("/api/background", HTTP_DELETE, handleDeleteBackground);
    server.on("/api/background", HTTP_POST, handleBackgroundDone, handleBackgroundUpload);
    server.on("/api/ota", HTTP_POST, handleOtaDone, handleOtaUpload);
    server.onNotFound(handleNotFound);
    server.begin();
}

void webLoop() {
    const bool apActive = wifiState() == WifiState::AccessPoint;
    if (apActive && !dnsRunning) {
        dns.start(53, "*", WiFi.softAPIP());
        dnsRunning = true;
    } else if (!apActive && dnsRunning) {
        dns.stop();
        dnsRunning = false;
    }
    if (dnsRunning) {
        dns.processNextRequest();
    }
    server.handleClient();
}

}  // namespace net
