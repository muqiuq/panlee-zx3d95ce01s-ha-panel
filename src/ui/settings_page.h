#pragma once

#include <lvgl.h>

namespace ui {

// Built-in options: brightness, sleep now, firmware version.
void settingsPageBuild(lv_obj_t* content);
void settingsPageTick();
void settingsPageReset();

}  // namespace ui
