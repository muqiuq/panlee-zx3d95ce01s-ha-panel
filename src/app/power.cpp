#include "power.h"

#include <lvgl.h>

#include "config.h"
#include "hw/display.h"
#include "ui/detail.h"
#include "ui/page_manager.h"

namespace app {

namespace {

PowerState state = PowerState::Awake;
uint32_t lastCheckMs = 0;
bool blanked = false;

void applyBacklight() {
    if (blanked || state == PowerState::Asleep) {
        hw::setBacklight(0);
    } else if (state == PowerState::Dimmed) {
        hw::setBacklight(min(config.display.dimBrightness, config.display.brightness));
    } else {
        hw::setBacklight(config.display.brightness);
    }
}

}  // namespace

void powerBlank(bool blank) {
    blanked = blank;
    applyBacklight();
}

void powerBegin() {
    state = PowerState::Awake;
    applyBacklight();
}

void powerLoop() {
    if (millis() - lastCheckMs < 200) {
        return;
    }
    lastCheckMs = millis();
    if (state == PowerState::Asleep) {
        return;
    }

    const uint32_t inactive = lv_display_get_inactive_time(nullptr);
    const auto& d = config.display;
    if (d.sleepSeconds > 0 && inactive >= d.sleepSeconds * 1000UL) {
        powerSleep();
    } else if (d.dimSeconds > 0 && inactive >= d.dimSeconds * 1000UL) {
        if (state == PowerState::Awake) {
            state = PowerState::Dimmed;
            applyBacklight();
        }
    } else if (state == PowerState::Dimmed) {
        state = PowerState::Awake;
        applyBacklight();
    }
}

PowerState powerState() {
    return state;
}

const char* powerStateName() {
    switch (state) {
        case PowerState::Dimmed: return "dimmed";
        case PowerState::Asleep: return "asleep";
        default: return "awake";
    }
}

bool powerSleeping() {
    return state == PowerState::Asleep;
}

void powerWake() {
    state = PowerState::Awake;
    lv_display_trigger_activity(nullptr);
    applyBacklight();
}

void powerSleep() {
    state = PowerState::Asleep;
    applyBacklight();
    ui::detailClose();
    if (config.display.returnToFirstPage) {
        ui::showPage(0);
    }
}

void powerApply() {
    applyBacklight();
}

}  // namespace app
