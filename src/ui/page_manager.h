#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace ui {

constexpr int kNavHeight = 52;

// Deletes all page screens (shows an empty screen meanwhile).
void pagesClear();
// Creates a page screen with background and navigation bar; returns the content area (480 x 428).
lv_obj_t* pageCreate(const String& name);

void showPage(size_t index);
void nextPage();
void prevPage();
size_t currentPage();
size_t pageCount();

}  // namespace ui
