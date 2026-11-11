#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

namespace app {

constexpr size_t kTilesPerPage = 4;
constexpr size_t kMaxPages = 10;

enum class TileType : uint8_t {
    Empty,
    Thermostat,
    Switch,
    Light,
    Cover,
    Sensor,
    BinarySensor,
    Scene,
    Button,
    Media,
    Lock,
    Fan,
    Clock,
    PageLink,
};

const char* tileTypeName(TileType type);
TileType tileTypeFromName(const char* name);

struct WifiConfig {
    String ssid;
    String password;
    String hostname;
    bool rebootOnLoss = true;
    uint16_t rebootAfterMinutes = 5;
};

struct MqttConfig {
    String host;
    uint16_t port = 1883;
    String user;
    String password;
    String baseTopic;
    bool discovery = true;
    String discoveryPrefix = "homeassistant";
};

struct DisplayConfig {
    uint16_t dimSeconds = 30;
    uint16_t sleepSeconds = 60;
    uint8_t brightness = 200;
    uint8_t dimBrightness = 40;
    bool returnToFirstPage = true;
};

struct ThemeConfig {
    String preset = "dark";
    String bg = "#101418";
    String tileBg = "#1E252D";
    String text = "#FFFFFF";
    String textSecondary = "#9AA4AE";
    String accent = "#4FC3F7";
    String on = "#FFB300";
    String off = "#5F6B76";
    String warning = "#E53935";
    String dialog = "#1A2129";
    uint8_t tileOpacity = 85;
};

struct TimeConfig {
    String timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
    String ntpServer = "pool.ntp.org";
};

struct WebConfig {
    String password;
};

// Topic meaning depends on the type; see TYPES in web/index.html for per-type labels.
struct TileConfig {
    TileType type = TileType::Empty;
    String title;
    String icon;
    String color;
    bool confirm = false;
    String stateTopic;
    String commandTopic;
    String valueTopic;
    String valueCommandTopic;
    String payloadOn = "ON";
    String payloadOff = "OFF";
    String payload;
    String labelOn;
    String labelOff;
    String unit;
    float min = 0;
    float max = 100;
    float step = 1;
    uint8_t decimals = 1;
    uint8_t page = 0;
};

struct PageConfig {
    String name;
    TileConfig tiles[kTilesPerPage];
};

struct Config {
    WifiConfig wifi;
    MqttConfig mqtt;
    DisplayConfig display;
    ThemeConfig theme;
    TimeConfig time;
    WebConfig web;
    std::vector<PageConfig> pages;
};

extern Config config;

String deviceId();
Config defaultConfig();

// Mounts LittleFS and loads /config.json (falls back to backup, then defaults).
void configBegin();
bool configSave();
void configFactoryReset();

void configToJson(const Config& c, JsonDocument& doc, bool maskSecrets);
// Merges `j` onto `c`; masked secrets keep their current value. Values are clamped.
void configFromJson(JsonVariantConst j, Config& c);

}  // namespace app
