#include "theme.h"

#include "app/config.h"

namespace ui {

namespace {

Theme current;

}  // namespace

const Theme& theme() {
    return current;
}

lv_color_t parseColor(const String& hex, lv_color_t fallback) {
    const char* s = hex.c_str();
    if (*s == '#') {
        ++s;
    }
    if (strlen(s) != 6) {
        return fallback;
    }
    char* end = nullptr;
    const uint32_t v = strtoul(s, &end, 16);
    return (end && *end == '\0') ? lv_color_hex(v) : fallback;
}

void themeApply() {
    const auto& c = app::config.theme;
    current.bg = parseColor(c.bg, lv_color_hex(0x101418));
    current.tileBg = parseColor(c.tileBg, lv_color_hex(0x1E252D));
    current.text = parseColor(c.text, lv_color_white());
    current.textSecondary = parseColor(c.textSecondary, lv_color_hex(0x9AA4AE));
    current.accent = parseColor(c.accent, lv_color_hex(0x4FC3F7));
    current.on = parseColor(c.on, lv_color_hex(0xFFB300));
    current.off = parseColor(c.off, lv_color_hex(0x5F6B76));
    current.warning = parseColor(c.warning, lv_color_hex(0xE53935));
    current.dialog = parseColor(c.dialog, lv_color_hex(0x1A2129));
    current.tileOpa = static_cast<lv_opa_t>(c.tileOpacity * 255 / 100);
    current.dark = lv_color_luminance(current.bg) < 128;

    lv_display_t* disp = lv_display_get_default();
    lv_theme_t* th = lv_theme_default_init(disp, current.accent, current.warning, current.dark,
                                           &lv_font_montserrat_20);
    lv_display_set_theme(disp, th);
}

}  // namespace ui
