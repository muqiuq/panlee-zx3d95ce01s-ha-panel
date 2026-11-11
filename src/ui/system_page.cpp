#include "system_page.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>

#include "app/config.h"
#include "hw/climate_sensor.h"
#include "detail.h"
#include "net/mqtt_service.h"
#include "net/wifi_manager.h"
#include "theme.h"
#include "version.h"

namespace ui {

namespace {

lv_obj_t* wifiLabel = nullptr;
lv_obj_t* mqttLabel = nullptr;
lv_obj_t* deviceLabel = nullptr;

const char* resetReasonName() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "Power on";
        case ESP_RST_EXT: return "External";
        case ESP_RST_SW: return "Software";
        case ESP_RST_PANIC: return "Panic";
        case ESP_RST_INT_WDT: return "Interrupt WDT";
        case ESP_RST_TASK_WDT: return "Task WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "Deep sleep";
        case ESP_RST_BROWNOUT: return "Brownout";
        default: return "Unknown";
    }
}

String sensorText() {
    if (!hw::climateAvailable()) {
        return "not detected";
    }
    if (isnan(app::panelTemperature())) {
        return "waiting...";
    }
    return String(app::panelTemperature(), 1) + " °C, " + String(app::panelHumidity(), 0) + " %";
}

lv_obj_t* addSection(lv_obj_t* parent, const char* title) {
    lv_obj_t* header = lv_label_create(parent);
    lv_label_set_text(header, title);
    lv_obj_set_style_text_font(header, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(header, theme().accent, 0);

    lv_obj_t* body = lv_label_create(parent);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_style_text_font(body, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(body, theme().text, 0);
    lv_obj_set_style_pad_bottom(body, 8, 0);
    return body;
}

}  // namespace

void systemPageTick() {
    if (!wifiLabel) {
        return;
    }
    if (net::wifiState() == net::WifiState::AccessPoint) {
        lv_label_set_text_fmt(wifiLabel,
                              "State: %s\nAP SSID: %s\nAP password: %s\nAP IP: %s\nMAC: %s",
                              net::wifiStateName(), net::apSsid().c_str(), net::apPassword().c_str(),
                              WiFi.softAPIP().toString().c_str(), WiFi.macAddress().c_str());
    } else {
        lv_label_set_text_fmt(wifiLabel,
                              "State: %s\nSSID: %s\nRSSI: %d dBm  Channel: %d\nIP: %s\nGateway: %s\n"
                              "DNS: %s\nHostname: %s\nMAC: %s\nWeb: http://%s/",
                              net::wifiStateName(), WiFi.SSID().c_str(), WiFi.RSSI(), WiFi.channel(),
                              WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(),
                              WiFi.dnsIP().toString().c_str(), app::config.wifi.hostname.c_str(),
                              WiFi.macAddress().c_str(), WiFi.localIP().toString().c_str());
    }

    const auto& mqtt = app::config.mqtt;
    lv_label_set_text_fmt(mqttLabel, "Broker: %s:%u\nBase topic: %s\nState: %s",
                          mqtt.host.isEmpty() ? "-" : mqtt.host.c_str(), mqtt.port, mqtt.baseTopic.c_str(),
                          net::mqttStateName());

    const uint32_t up = millis() / 1000;
    lv_label_set_text_fmt(deviceLabel,
                          "Firmware: %s (%s %s)\nUptime: %lud %02lu:%02lu:%02lu\n"
                          "Heap: %lu / %lu KB free\nPSRAM: %lu / %lu KB free\n"
                          "Flash: %lu MB  Chip: %s rev %d, %d cores @ %lu MHz\nReset reason: %s\nSensor: %s",
                          FIRMWARE_VERSION, __DATE__, __TIME__,
                          (unsigned long)(up / 86400), (unsigned long)(up / 3600 % 24),
                          (unsigned long)(up / 60 % 60), (unsigned long)(up % 60),
                          (unsigned long)(ESP.getFreeHeap() / 1024), (unsigned long)(ESP.getHeapSize() / 1024),
                          (unsigned long)(ESP.getFreePsram() / 1024), (unsigned long)(ESP.getPsramSize() / 1024),
                          (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024)), ESP.getChipModel(),
                          ESP.getChipRevision(), ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz(),
                          resetReasonName(), sensorText().c_str());
}

namespace {

void rebootClicked(lv_event_t*) {
    runConfirmed(true, "Reboot the panel now?", []() { ESP.restart(); });
}

}  // namespace

void systemPageBuild(lv_obj_t* content) {
    lv_obj_t* scr = lv_obj_create(content);
    lv_obj_set_size(scr, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(scr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 16, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(scr, LV_DIR_VER);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS "  System");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, theme().text, 0);
    lv_obj_set_style_pad_bottom(title, 8, 0);

    wifiLabel = addSection(scr, LV_SYMBOL_WIFI "  WiFi");
    mqttLabel = addSection(scr, LV_SYMBOL_UPLOAD "  MQTT");
    deviceLabel = addSection(scr, LV_SYMBOL_LIST "  Device");

    lv_obj_t* about = addSection(scr, LV_SYMBOL_HOME "  About");
    lv_label_set_text(about, "Philipp Albrecht\nphilipp@uisa.ch\nwww.uisa.ch");

    lv_obj_t* btn = lv_button_create(scr);
    lv_obj_set_size(btn, lv_pct(100), 56);
    lv_obj_set_style_bg_color(btn, theme().warning, 0);
    lv_obj_add_event_cb(btn, rebootClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* btnLabel = lv_label_create(btn);
    lv_label_set_text(btnLabel, LV_SYMBOL_REFRESH "  Reboot");
    lv_obj_set_style_text_font(btnLabel, &lv_font_montserrat_20, 0);
    lv_obj_center(btnLabel);

    systemPageTick();
}

void systemPageReset() {
    wifiLabel = nullptr;
    mqttLabel = nullptr;
    deviceLabel = nullptr;
}

}  // namespace ui
