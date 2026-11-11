#include "climate_sensor.h"

#include "display.h"

namespace hw {

namespace {

constexpr int kPort = 1;
constexpr int kSda = 15;
constexpr int kScl = 6;
constexpr uint8_t kAddr = 0x40;
constexpr uint32_t kFreq = 100000;
constexpr uint32_t kIntervalMs = 10000;

constexpr uint8_t kCmdTemp = 0xF3;      // no hold master
constexpr uint8_t kCmdHumidity = 0xF5;  // no hold master
constexpr uint8_t kCmdSoftReset = 0xFE;

enum class Step { Idle, WaitTemp, WaitHumidity };

bool present = false;
Step step = Step::Idle;
uint32_t stepStartMs = 0;
uint32_t lastCycleMs = 0;
float temperature = NAN;
float humidity = NAN;

bool crcOk(const uint8_t* d) {
    uint8_t crc = 0;
    for (int i = 0; i < 2; ++i) {
        crc ^= d[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31) : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc == d[2];
}

bool command(uint8_t cmd) {
    return lgfx::i2c::transactionWrite(kPort, kAddr, &cmd, 1, kFreq).has_value();
}

bool readRaw(uint16_t& raw) {
    uint8_t d[3];
    if (!lgfx::i2c::transactionRead(kPort, kAddr, d, 3, kFreq).has_value() || !crcOk(d)) {
        return false;
    }
    raw = static_cast<uint16_t>((d[0] << 8) | d[1]) & 0xFFFC;
    return true;
}

void logBusScan() {
    String found;
    for (uint8_t a = 0x08; a < 0x78; ++a) {
        if (lgfx::i2c::transactionWrite(kPort, a, nullptr, 0, kFreq).has_value()) {
            found += String(" 0x") + String(a, HEX);
        }
    }
    Serial.printf("SHT20 not found; I2C%d devices:%s\n", kPort, found.isEmpty() ? " none" : found.c_str());
}

}  // namespace

bool climateBegin(bool busReady) {
    if (!busReady && !lgfx::i2c::init(kPort, kSda, kScl).has_value()) {
        return false;
    }
    present = command(kCmdSoftReset);
    if (!present) {
        logBusScan();
        return false;
    }
    // SHT20 needs up to 15 ms after a soft reset.
    lastCycleMs = millis() - kIntervalMs + 50;
    return true;
}

void climateLoop() {
    if (!present) {
        return;
    }
    const uint32_t now = millis();
    uint16_t raw;
    switch (step) {
        case Step::Idle:
            if (now - lastCycleMs >= kIntervalMs) {
                lastCycleMs = now;
                if (command(kCmdTemp)) {
                    step = Step::WaitTemp;
                    stepStartMs = now;
                }
            }
            break;
        case Step::WaitTemp:
            if (now - stepStartMs >= 90) {
                if (readRaw(raw)) {
                    temperature = -46.85f + 175.72f * raw / 65536.0f;
                }
                step = command(kCmdHumidity) ? Step::WaitHumidity : Step::Idle;
                stepStartMs = now;
            }
            break;
        case Step::WaitHumidity:
            if (now - stepStartMs >= 35) {
                if (readRaw(raw)) {
                    humidity = constrain(-6.0f + 125.0f * raw / 65536.0f, 0.0f, 100.0f);
                }
                step = Step::Idle;
            }
            break;
    }
}

bool climateAvailable() {
    return present;
}

float climateTemperature() {
    return temperature;
}

float climateHumidity() {
    return humidity;
}

}  // namespace hw
