#pragma once

#include "config.h"

namespace app {

constexpr uint32_t kWatchdogSeconds = 30;

void setup();
void loop();

// Seconds since WiFi was last connected (0 while connected or when the reboot feature is off).
uint32_t wifiOfflineSeconds();

// Applies `config` after it changed; `previous` is used to detect what needs restarting.
void applyConfig(const Config& previous);
void scheduleRestart(uint32_t delayMs);

}  // namespace app
