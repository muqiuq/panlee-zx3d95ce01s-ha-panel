#pragma once

#include <lvgl.h>

namespace ui {

// Raw little-endian RGB565, 480x480, produced by the web interface.
constexpr const char* kBackgroundPath = "/bg.bin";
constexpr size_t kBackgroundBytes = 480 * 480 * 2;

// (Re)loads the background into PSRAM. Must not be called while screens reference the old image.
void backgroundLoad();
const lv_image_dsc_t* backgroundImage();

}  // namespace ui
