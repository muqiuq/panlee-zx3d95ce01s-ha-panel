#pragma once

#include "config.h"

namespace app {

void setup();
void loop();

// Applies `config` after it changed; `previous` is used to detect what needs restarting.
void applyConfig(const Config& previous);
void scheduleRestart(uint32_t delayMs);

}  // namespace app
