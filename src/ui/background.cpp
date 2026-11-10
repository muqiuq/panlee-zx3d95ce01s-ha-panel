#include "background.h"

#include <LittleFS.h>
#include <esp_heap_caps.h>

namespace ui {

namespace {

uint8_t* pixels = nullptr;
lv_image_dsc_t dsc;
bool loaded = false;

}  // namespace

void backgroundLoad() {
    if (loaded) {
        lv_image_cache_drop(&dsc);
    }
    loaded = false;
    if (pixels) {
        heap_caps_free(pixels);
        pixels = nullptr;
    }

    File f = LittleFS.open(kBackgroundPath, "r");
    if (!f) {
        return;
    }
    if (f.size() != kBackgroundBytes) {
        log_w("Background has wrong size %u", static_cast<unsigned>(f.size()));
        f.close();
        return;
    }
    pixels = static_cast<uint8_t*>(heap_caps_malloc(kBackgroundBytes, MALLOC_CAP_SPIRAM));
    if (!pixels) {
        f.close();
        return;
    }
    const size_t read = f.read(pixels, kBackgroundBytes);
    f.close();
    if (read != kBackgroundBytes) {
        heap_caps_free(pixels);
        pixels = nullptr;
        return;
    }

    memset(&dsc, 0, sizeof(dsc));
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    dsc.header.w = 480;
    dsc.header.h = 480;
    dsc.header.stride = 480 * 2;
    dsc.data_size = kBackgroundBytes;
    dsc.data = pixels;
    loaded = true;
}

const lv_image_dsc_t* backgroundImage() {
    return loaded ? &dsc : nullptr;
}

}  // namespace ui
