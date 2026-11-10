#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace ui {

struct Theme {
    lv_color_t bg;
    lv_color_t tileBg;
    lv_color_t text;
    lv_color_t textSecondary;
    lv_color_t accent;
    lv_color_t on;
    lv_color_t off;
    lv_color_t warning;
    lv_color_t dialog;
    lv_opa_t tileOpa;
    bool dark;
};

const Theme& theme();
// Loads colors from app::config.theme and applies them to the LVGL default theme.
void themeApply();
lv_color_t parseColor(const String& hex, lv_color_t fallback);

}  // namespace ui
