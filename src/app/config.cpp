#include "config.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "hw/climate_sensor.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

namespace app {

Config config;

namespace {

constexpr const char* kPath = "/config.json";
constexpr const char* kBackupPath = "/config.bak.json";
constexpr const char* kTmpPath = "/config.tmp.json";

constexpr const char* kMask = "********";

const char* const kTypeNames[] = {
    "empty", "thermostat", "switch", "light", "cover", "sensor", "binary_sensor",
    "scene", "button", "media", "lock", "fan", "clock", "page_link", "panel_climate",
};

String limited(JsonVariantConst v, const String& fallback, size_t maxLen) {
    if (!v.is<const char*>()) {
        return fallback;
    }
    String s = v.as<const char*>();
    if (s.length() > maxLen) {
        s = s.substring(0, maxLen);
    }
    return s;
}

void readStr(JsonVariantConst v, String& target, size_t maxLen = 64) {
    target = limited(v, target, maxLen);
}

void readSecret(JsonVariantConst v, String& target) {
    if (v.is<const char*>() && strcmp(v.as<const char*>(), kMask) != 0) {
        target = limited(v, target, 64);
    }
}

template <typename T>
void readNum(JsonVariantConst v, T& target, T lo, T hi) {
    if (v.is<float>() || v.is<long>()) {
        target = static_cast<T>(constrain(v.as<float>(), static_cast<float>(lo), static_cast<float>(hi)));
    }
}

void readBool(JsonVariantConst v, bool& target) {
    if (v.is<bool>()) {
        target = v.as<bool>();
    }
}

void putStr(JsonObject o, const char* key, const String& value) {
    if (!value.isEmpty()) {
        o[key] = value;
    }
}

void putSecret(JsonObject o, const char* key, const String& value, bool mask) {
    o[key] = (mask && !value.isEmpty()) ? String(kMask) : value;
}

void tileFromJson(JsonVariantConst j, TileConfig& t) {
    t.type = tileTypeFromName(j["type"] | "empty");
    readStr(j["title"], t.title, 32);
    readStr(j["icon"], t.icon, 16);
    readStr(j["color"], t.color, 9);
    readBool(j["confirm"], t.confirm);
    readStr(j["stateTopic"], t.stateTopic, 128);
    readStr(j["commandTopic"], t.commandTopic, 128);
    readStr(j["valueTopic"], t.valueTopic, 128);
    readStr(j["valueCommandTopic"], t.valueCommandTopic, 128);
    readStr(j["payloadOn"], t.payloadOn, 32);
    readStr(j["payloadOff"], t.payloadOff, 32);
    readStr(j["payload"], t.payload, 128);
    readStr(j["labelOn"], t.labelOn, 16);
    readStr(j["labelOff"], t.labelOff, 16);
    readStr(j["unit"], t.unit, 8);
    readNum(j["min"], t.min, -10000.0f, 10000.0f);
    readNum(j["max"], t.max, -10000.0f, 10000.0f);
    readNum(j["step"], t.step, 0.01f, 1000.0f);
    readNum<uint8_t>(j["decimals"], t.decimals, 0, 3);
    readNum<uint8_t>(j["page"], t.page, 0, kMaxPages - 1);
    if (t.max <= t.min) {
        t.max = t.min + 1;
    }
}

void tileToJson(const TileConfig& t, JsonObject o) {
    o["type"] = tileTypeName(t.type);
    if (t.type == TileType::Empty) {
        return;
    }
    putStr(o, "title", t.title);
    putStr(o, "icon", t.icon);
    putStr(o, "color", t.color);
    o["confirm"] = t.confirm;
    putStr(o, "stateTopic", t.stateTopic);
    putStr(o, "commandTopic", t.commandTopic);
    putStr(o, "valueTopic", t.valueTopic);
    putStr(o, "valueCommandTopic", t.valueCommandTopic);
    o["payloadOn"] = t.payloadOn;
    o["payloadOff"] = t.payloadOff;
    putStr(o, "payload", t.payload);
    putStr(o, "labelOn", t.labelOn);
    putStr(o, "labelOff", t.labelOff);
    putStr(o, "unit", t.unit);
    o["min"] = t.min;
    o["max"] = t.max;
    o["step"] = t.step;
    o["decimals"] = t.decimals;
    o["page"] = t.page;
}

bool loadFile(const char* path, Config& c) {
    File f = LittleFS.open(path, "r");
    if (!f) {
        return false;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        log_w("Config %s invalid: %s", path, err.c_str());
        return false;
    }
    configFromJson(doc.as<JsonVariantConst>(), c);
    return true;
}

}  // namespace

const char* tileTypeName(TileType type) {
    const auto i = static_cast<size_t>(type);
    return i < sizeof(kTypeNames) / sizeof(kTypeNames[0]) ? kTypeNames[i] : "empty";
}

TileType tileTypeFromName(const char* name) {
    for (size_t i = 0; i < sizeof(kTypeNames) / sizeof(kTypeNames[0]); ++i) {
        if (strcmp(name, kTypeNames[i]) == 0) {
            return static_cast<TileType>(i);
        }
    }
    return TileType::Empty;
}

Config defaultConfig() {
    Config c;
    c.wifi.hostname = "ha-panel-" + deviceId();
    c.mqtt.baseTopic = "panel/" + deviceId();
#ifdef SECRET_WIFI_SSID
    c.wifi.ssid = SECRET_WIFI_SSID;
    c.wifi.password = SECRET_WIFI_PASSWORD;
#endif
#ifdef SECRET_MQTT_HOST
    c.mqtt.host = SECRET_MQTT_HOST;
    c.mqtt.user = SECRET_MQTT_USER;
    c.mqtt.password = SECRET_MQTT_PASSWORD;
#endif
    PageConfig home;
    home.name = "Home";
    home.tiles[0].type = TileType::Clock;
    home.tiles[0].title = "Clock";
    c.pages.push_back(home);
    return c;
}

void configFromJson(JsonVariantConst j, Config& c) {
    JsonVariantConst w = j["wifi"];
    readStr(w["ssid"], c.wifi.ssid, 32);
    readSecret(w["password"], c.wifi.password);
    readStr(w["hostname"], c.wifi.hostname, 32);
    readBool(w["rebootOnLoss"], c.wifi.rebootOnLoss);
    readNum<uint16_t>(w["rebootAfterMinutes"], c.wifi.rebootAfterMinutes, 1, 1440);

    JsonVariantConst m = j["mqtt"];
    readStr(m["host"], c.mqtt.host, 64);
    readNum<uint16_t>(m["port"], c.mqtt.port, 1, 65535);
    readStr(m["user"], c.mqtt.user, 64);
    readSecret(m["password"], c.mqtt.password);
    readStr(m["baseTopic"], c.mqtt.baseTopic, 64);
    readBool(m["discovery"], c.mqtt.discovery);
    readStr(m["discoveryPrefix"], c.mqtt.discoveryPrefix, 32);
    while (c.mqtt.baseTopic.endsWith("/")) {
        c.mqtt.baseTopic.remove(c.mqtt.baseTopic.length() - 1);
    }

    JsonVariantConst d = j["display"];
    readNum<uint16_t>(d["dimSeconds"], c.display.dimSeconds, 0, 3600);
    readNum<uint16_t>(d["sleepSeconds"], c.display.sleepSeconds, 0, 3600);
    readNum<uint8_t>(d["brightness"], c.display.brightness, 10, 255);
    readNum<uint8_t>(d["dimBrightness"], c.display.dimBrightness, 1, 255);
    readBool(d["returnToFirstPage"], c.display.returnToFirstPage);

    JsonVariantConst th = j["theme"];
    readStr(th["preset"], c.theme.preset, 16);
    readStr(th["bg"], c.theme.bg, 9);
    readStr(th["tileBg"], c.theme.tileBg, 9);
    readStr(th["text"], c.theme.text, 9);
    readStr(th["textSecondary"], c.theme.textSecondary, 9);
    readStr(th["accent"], c.theme.accent, 9);
    readStr(th["on"], c.theme.on, 9);
    readStr(th["off"], c.theme.off, 9);
    readStr(th["warning"], c.theme.warning, 9);
    readStr(th["dialog"], c.theme.dialog, 9);
    readNum<uint8_t>(th["tileOpacity"], c.theme.tileOpacity, 0, 100);

    JsonVariantConst t = j["time"];
    readStr(t["timezone"], c.time.timezone, 64);
    readStr(t["ntpServer"], c.time.ntpServer, 64);

    readSecret(j["web"]["password"], c.web.password);

    JsonVariantConst s = j["sensor"];
    readNum(s["temperatureOffset"], c.sensor.temperatureOffset, -20.0f, 20.0f);
    readNum(s["humidityOffset"], c.sensor.humidityOffset, -50.0f, 50.0f);
    readNum<uint16_t>(s["publishSeconds"], c.sensor.publishSeconds, 10, 3600);
    readNum(s["temperatureDelta"], c.sensor.temperatureDelta, 0.0f, 10.0f);
    readNum(s["humidityDelta"], c.sensor.humidityDelta, 0.0f, 50.0f);

    JsonArrayConst pages = j["pages"];
    if (!pages.isNull() && pages.size() > 0) {
        c.pages.clear();
        for (JsonVariantConst p : pages) {
            if (c.pages.size() >= kMaxPages) {
                break;
            }
            PageConfig page;
            readStr(p["name"], page.name, 24);
            JsonArrayConst tiles = p["tiles"];
            for (size_t i = 0; i < kTilesPerPage && i < tiles.size(); ++i) {
                tileFromJson(tiles[i], page.tiles[i]);
            }
            c.pages.push_back(page);
        }
    }
}

void configToJson(const Config& c, JsonDocument& doc, bool maskSecrets) {
    JsonObject w = doc["wifi"].to<JsonObject>();
    w["ssid"] = c.wifi.ssid;
    putSecret(w, "password", c.wifi.password, maskSecrets);
    w["hostname"] = c.wifi.hostname;
    w["rebootOnLoss"] = c.wifi.rebootOnLoss;
    w["rebootAfterMinutes"] = c.wifi.rebootAfterMinutes;

    JsonObject m = doc["mqtt"].to<JsonObject>();
    m["host"] = c.mqtt.host;
    m["port"] = c.mqtt.port;
    m["user"] = c.mqtt.user;
    putSecret(m, "password", c.mqtt.password, maskSecrets);
    m["baseTopic"] = c.mqtt.baseTopic;
    m["discovery"] = c.mqtt.discovery;
    m["discoveryPrefix"] = c.mqtt.discoveryPrefix;

    JsonObject d = doc["display"].to<JsonObject>();
    d["dimSeconds"] = c.display.dimSeconds;
    d["sleepSeconds"] = c.display.sleepSeconds;
    d["brightness"] = c.display.brightness;
    d["dimBrightness"] = c.display.dimBrightness;
    d["returnToFirstPage"] = c.display.returnToFirstPage;

    JsonObject th = doc["theme"].to<JsonObject>();
    th["preset"] = c.theme.preset;
    th["bg"] = c.theme.bg;
    th["tileBg"] = c.theme.tileBg;
    th["text"] = c.theme.text;
    th["textSecondary"] = c.theme.textSecondary;
    th["accent"] = c.theme.accent;
    th["on"] = c.theme.on;
    th["off"] = c.theme.off;
    th["warning"] = c.theme.warning;
    th["dialog"] = c.theme.dialog;
    th["tileOpacity"] = c.theme.tileOpacity;

    JsonObject t = doc["time"].to<JsonObject>();
    t["timezone"] = c.time.timezone;
    t["ntpServer"] = c.time.ntpServer;

    putSecret(doc["web"].to<JsonObject>(), "password", c.web.password, maskSecrets);

    JsonObject s = doc["sensor"].to<JsonObject>();
    s["temperatureOffset"] = c.sensor.temperatureOffset;
    s["humidityOffset"] = c.sensor.humidityOffset;
    s["publishSeconds"] = c.sensor.publishSeconds;
    s["temperatureDelta"] = c.sensor.temperatureDelta;
    s["humidityDelta"] = c.sensor.humidityDelta;

    JsonArray pages = doc["pages"].to<JsonArray>();
    for (const PageConfig& p : c.pages) {
        JsonObject po = pages.add<JsonObject>();
        po["name"] = p.name;
        JsonArray tiles = po["tiles"].to<JsonArray>();
        for (const TileConfig& tile : p.tiles) {
            tileToJson(tile, tiles.add<JsonObject>());
        }
    }
}

float panelTemperature() {
    return hw::climateTemperature() + config.sensor.temperatureOffset;
}

float panelHumidity() {
    return constrain(hw::climateHumidity() + config.sensor.humidityOffset, 0.0f, 100.0f);
}

String deviceId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[7];
    snprintf(buf, sizeof(buf), "%02x%02x%02x", mac[3], mac[4], mac[5]);
    return buf;
}

void configBegin() {
    if (!LittleFS.begin(true)) {
        log_e("LittleFS mount failed");
    }
    config = defaultConfig();
    if (loadFile(kPath, config)) {
        return;
    }
    config = defaultConfig();
    if (loadFile(kBackupPath, config)) {
        log_w("Config restored from backup");
    }
}

bool configSave() {
    JsonDocument doc;
    configToJson(config, doc, false);

    File f = LittleFS.open(kTmpPath, "w");
    if (!f) {
        return false;
    }
    const bool ok = serializeJson(doc, f) > 0;
    f.close();
    if (!ok) {
        LittleFS.remove(kTmpPath);
        return false;
    }

    LittleFS.remove(kBackupPath);
    if (LittleFS.exists(kPath)) {
        LittleFS.rename(kPath, kBackupPath);
    }
    return LittleFS.rename(kTmpPath, kPath);
}

void configFactoryReset() {
    LittleFS.remove(kPath);
    LittleFS.remove(kBackupPath);
    config = defaultConfig();
}

}  // namespace app
