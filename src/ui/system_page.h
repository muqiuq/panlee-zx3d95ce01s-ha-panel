#pragma once

#include <lvgl.h>

namespace ui {

// Technical details, reboot and author info.
void systemPageBuild(lv_obj_t* content);
void systemPageTick();
// Forget widget pointers before the page is deleted.
void systemPageReset();

}  // namespace ui
