#pragma once

#include <Arduino.h>

namespace app {

enum class PowerState { Awake, Dimmed, Asleep };

void powerBegin();
void powerLoop();
PowerState powerState();
const char* powerStateName();
bool powerSleeping();
void powerWake();
void powerSleep();
// Re-applies the configured brightness (e.g. after a config change).
void powerApply();
// Flash writes stall the PSRAM framebuffer and make the RGB panel flicker; keep the backlight off meanwhile.
void powerBlank(bool blank);

}  // namespace app
