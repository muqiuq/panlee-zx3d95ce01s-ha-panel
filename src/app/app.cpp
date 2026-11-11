#include "app.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "hw/display.h"
#include "net/mqtt_service.h"
#include "net/web_portal.h"
#include "net/wifi_manager.h"
#include "power.h"
#include "ui/background.h"
#include "ui/lvgl_port.h"
#include "ui/message.h"
#include "ui/page_manager.h"
#include "ui/ui.h"
#include "version.h"

namespace app {

namespace {

constexpr uint32_t kTelemetryIntervalMs = 60000;

bool uiReady = false;
uint32_t restartAtMs = 0;
uint32_t lastTelemetryMs = 0;
uint32_t seenConnections = 0;
bool mdnsStarted = false;
uint32_t lastWifiOkMs = 0;

void configureTime() {
    configTzTime(config.time.timezone.c_str(), config.time.ntpServer.c_str());
}

void configureMqtt() {
    net::MqttSettings s;
    s.host = config.mqtt.host;
    s.port = config.mqtt.port;
    s.user = config.mqtt.user;
    s.password = config.mqtt.password;
    s.clientId = config.wifi.hostname;
    s.baseTopic = config.mqtt.baseTopic;
    s.topics = ui::subscriptions();
    net::mqttConfigure(s);
}

void publishTelemetry() {
    JsonDocument doc;
    doc["rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    doc["uptime"] = millis() / 1000;
    doc["heap"] = ESP.getFreeHeap();
    doc["psram"] = ESP.getFreePsram();
    doc["brightness"] = config.display.brightness;
    doc["display"] = powerStateName();
    doc["page"] = ui::currentPage();
    String out;
    serializeJson(doc, out);
    net::mqttPublish(config.mqtt.baseTopic + "/telemetry", out, true);
}

void publishDiscoveryEntity(const char* component, const char* key, JsonDocument& doc) {
    const String id = "panel_" + deviceId();
    const String base = config.mqtt.baseTopic;
    doc["unique_id"] = id + "_" + key;
    doc["object_id"] = config.wifi.hostname + "_" + key;
    doc["availability_topic"] = base + "/status";
    JsonObject dev = doc["device"].to<JsonObject>();
    dev["identifiers"].to<JsonArray>().add(id);
    dev["name"] = config.wifi.hostname;
    dev["manufacturer"] = "uisa.ch";
    dev["model"] = "Panlee ZX3D95CE01S-TR";
    dev["sw_version"] = FIRMWARE_VERSION;
    dev["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";

    String payload;
    serializeJson(doc, payload);
    net::mqttPublish(config.mqtt.discoveryPrefix + "/" + component + "/" + id + "/" + key + "/config", payload, true);
}

void publishDiscovery() {
    if (!config.mqtt.discovery) {
        return;
    }
    const String base = config.mqtt.baseTopic;
    const String telemetry = base + "/telemetry";
    {
        JsonDocument d;
        d["name"] = "WiFi signal";
        d["state_topic"] = telemetry;
        d["value_template"] = "{{ value_json.rssi }}";
        d["unit_of_measurement"] = "dBm";
        d["device_class"] = "signal_strength";
        d["state_class"] = "measurement";
        d["entity_category"] = "diagnostic";
        publishDiscoveryEntity("sensor", "rssi", d);
    }
    {
        JsonDocument d;
        d["name"] = "Uptime";
        d["state_topic"] = telemetry;
        d["value_template"] = "{{ value_json.uptime }}";
        d["unit_of_measurement"] = "s";
        d["device_class"] = "duration";
        d["entity_category"] = "diagnostic";
        publishDiscoveryEntity("sensor", "uptime", d);
    }
    {
        JsonDocument d;
        d["name"] = "Display";
        d["state_topic"] = telemetry;
        d["value_template"] = "{{ value_json.display }}";
        publishDiscoveryEntity("sensor", "display", d);
    }
    {
        JsonDocument d;
        d["name"] = "Brightness";
        d["state_topic"] = telemetry;
        d["value_template"] = "{{ value_json.brightness }}";
        d["command_topic"] = base + "/cmd/brightness";
        d["min"] = 10;
        d["max"] = 255;
        d["entity_category"] = "config";
        publishDiscoveryEntity("number", "brightness", d);
    }
    {
        JsonDocument d;
        d["name"] = "Reboot";
        d["command_topic"] = base + "/cmd/reboot";
        d["payload_press"] = "1";
        d["device_class"] = "restart";
        d["entity_category"] = "config";
        publishDiscoveryEntity("button", "reboot", d);
    }
    {
        JsonDocument d;
        d["name"] = "Wake display";
        d["command_topic"] = base + "/cmd/wake";
        d["payload_press"] = "1";
        publishDiscoveryEntity("button", "wake", d);
    }
    {
        JsonDocument d;
        d["name"] = "Message";
        d["command_topic"] = base + "/cmd/message";
        publishDiscoveryEntity("notify", "message", d);
    }
}

void handleCommand(const String& cmd, const String& payload) {
    if (cmd == "reboot") {
        scheduleRestart(500);
    } else if (cmd == "wake") {
        powerWake();
    } else if (cmd == "sleep") {
        powerSleep();
    } else if (cmd == "brightness") {
        config.display.brightness = constrain(payload.toInt(), 10, 255);
        powerApply();
        publishTelemetry();
    } else if (cmd == "page") {
        const long page = payload.toInt();
        if (page >= 1) {
            powerWake();
            ui::showPage(static_cast<size_t>(page - 1));
        }
    } else if (cmd == "message") {
        String error;
        if (!ui::messageShow(payload, error)) {
            log_w("Message rejected: %s", error.c_str());
        }
    }
}

void processMqtt() {
    const String cmdPrefix = config.mqtt.baseTopic + "/cmd/";
    net::MqttMessage msg;
    for (int i = 0; i < 16 && net::mqttPoll(msg); ++i) {
        if (msg.topic.startsWith(cmdPrefix)) {
            handleCommand(msg.topic.substring(cmdPrefix.length()), msg.payload);
        } else {
            ui::dispatch(msg.topic, msg.payload);
        }
    }

    const uint32_t connections = net::mqttConnectionCount();
    if (connections != seenConnections) {
        seenConnections = connections;
        publishDiscovery();
        publishTelemetry();
        lastTelemetryMs = millis();
    }
    if (net::mqttConnected() && millis() - lastTelemetryMs > kTelemetryIntervalMs) {
        lastTelemetryMs = millis();
        publishTelemetry();
    }
}

void maintainMdns() {
    if (!mdnsStarted && net::wifiState() == net::WifiState::Connected) {
        mdnsStarted = MDNS.begin(config.wifi.hostname.c_str());
        if (mdnsStarted) {
            MDNS.addService("http", "tcp", 80);
        }
    }
}

void maintainWifiWatchdog() {
    const auto& w = config.wifi;
    // Someone using the setup AP must not be interrupted by a reboot.
    if (!w.rebootOnLoss || w.ssid.isEmpty() || net::wifiState() == net::WifiState::Connected ||
        WiFi.softAPgetStationNum() > 0) {
        lastWifiOkMs = millis();
        return;
    }
    if (millis() - lastWifiOkMs > w.rebootAfterMinutes * 60000UL) {
        Serial.printf("WiFi lost for %u min, rebooting\n", w.rebootAfterMinutes);
        ESP.restart();
    }
}

}  // namespace

uint32_t wifiOfflineSeconds() {
    return (millis() - lastWifiOkMs) / 1000;
}

void setup() {
    Serial.begin(115200);

    // Reboot if the main loop hangs for 30 s.
    esp_task_wdt_init(kWatchdogSeconds, true);
    esp_task_wdt_add(nullptr);

    configBegin();
    net::wifiBegin();
    configureTime();

    if (!hw::displayBegin()) {
        Serial.println("Display init failed");
    } else if (!ui::lvglBegin()) {
        Serial.println("LVGL init failed");
    } else {
        ui::backgroundLoad();
        ui::build();
        ui::showPage(0);
        powerBegin();
        uiReady = true;
    }

    net::mqttBegin();
    configureMqtt();
    net::webBegin();

    Serial.printf("Ready. Heap %u, PSRAM %u\n", ESP.getFreeHeap(), ESP.getFreePsram());
}

void loop() {
    esp_task_wdt_reset();
    net::wifiLoop();
    net::webLoop();
    maintainMdns();
    maintainWifiWatchdog();
    processMqtt();
    if (uiReady) {
        powerLoop();
        ui::lvglLoop();
    }
    if (restartAtMs && static_cast<int32_t>(millis() - restartAtMs) >= 0) {
        ESP.restart();
    }
    delay(5);
}

void applyConfig(const Config& previous) {
    if (uiReady) {
        ui::rebuild(false);
        powerApply();
    }
    if (previous.wifi.ssid != config.wifi.ssid || previous.wifi.password != config.wifi.password ||
        previous.wifi.hostname != config.wifi.hostname) {
        mdnsStarted = false;
        MDNS.end();
        net::wifiBegin();
    }
    if (previous.time.timezone != config.time.timezone || previous.time.ntpServer != config.time.ntpServer) {
        configureTime();
    }
    configureMqtt();
}

void scheduleRestart(uint32_t delayMs) {
    restartAtMs = millis() + delayMs;
    if (restartAtMs == 0) {
        restartAtMs = 1;
    }
}

}  // namespace app
