#include "lvgl_port.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "app/power.h"
#include "hw/display.h"

namespace ui {

namespace {

constexpr int kBufferLines = 48;

bool swallowTouch = false;

uint32_t tickCb() {
    return millis();
}

void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap) {
    const int32_t w = lv_area_get_width(area);
    const int32_t h = lv_area_get_height(area);
    hw::display.pushImage(area->x1, area->y1, w, h, reinterpret_cast<const lgfx::rgb565_t*>(pxMap));
    lv_display_flush_ready(disp);
}

void touchReadCb(lv_indev_t*, lv_indev_data_t* data) {
    uint16_t x;
    uint16_t y;
    if (hw::display.getTouch(&x, &y)) {
        // The touch that wakes the display must not trigger any action.
        if (app::powerSleeping()) {
            app::powerWake();
            swallowTouch = true;
        }
        if (swallowTouch) {
            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        swallowTouch = false;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

}  // namespace

bool lvglBegin() {
    lv_init();
    lv_tick_set_cb(tickCb);

    const size_t bufBytes = hw::kScreenWidth * kBufferLines * sizeof(uint16_t);
    void* buf1 = heap_caps_malloc(bufBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    void* buf2 = heap_caps_malloc(bufBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buf1 || !buf2) {
        return false;
    }

    lv_display_t* disp = lv_display_create(hw::kScreenWidth, hw::kScreenHeight);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, buf1, buf2, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flushCb);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchReadCb);
    return true;
}

void lvglLoop() {
    lv_timer_handler();
}

}  // namespace ui
