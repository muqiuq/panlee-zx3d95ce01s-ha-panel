#include "settings_page.h"

#include <Arduino.h>

#include "app/config.h"
#include "app/power.h"
#include "message.h"
#include "theme.h"
#include "version.h"

namespace ui {

namespace {

lv_obj_t* slider = nullptr;
lv_obj_t* valueLabel = nullptr;
lv_obj_t* messageLabel = nullptr;
uint32_t shownRevision = UINT32_MAX;
uint32_t lastApplyMs = 0;

int percent(int level) {
    return (level * 100 + 127) / 255;
}

void updateLabel() {
    lv_label_set_text_fmt(valueLabel, "%d%%", percent(lv_slider_get_value(slider)));
}

void applyBrightness() {
    app::config.display.brightness = static_cast<uint8_t>(lv_slider_get_value(slider));
    app::powerApply();
    lastApplyMs = millis();
}

void sliderChanged(lv_event_t*) {
    updateLabel();
    if (millis() - lastApplyMs >= 80) {
        applyBrightness();
    }
}

void sliderReleased(lv_event_t*) {
    applyBrightness();
}

void sleepClicked(lv_event_t*) {
    app::powerSleep();
}

lv_obj_t* sectionLabel(lv_obj_t* parent, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l, theme().textSecondary, 0);
    return l;
}

void updateMessage() {
    shownRevision = messageRevision();
    const MessageInfo& m = lastMessage();
    if (m.time.isEmpty()) {
        lv_label_set_text(messageLabel, "No messages");
        return;
    }
    String s = m.time + "  " + m.title;
    if (!m.text.isEmpty()) {
        s += "\n" + m.text;
    }
    s += "\n" + (m.open ? String("(open)") : "Answer: " + m.answer);
    lv_label_set_text(messageLabel, s.c_str());
}

}  // namespace

void settingsPageBuild(lv_obj_t* content) {
    const Theme& th = theme();
    lv_obj_t* box = lv_obj_create(content);
    lv_obj_set_size(box, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 20, 0);
    lv_obj_set_style_pad_row(box, 14, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(box, LV_DIR_VER);

    lv_obj_t* title = lv_label_create(box);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS "  Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, th.text, 0);

    lv_obj_t* row = lv_obj_create(box);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* label = sectionLabel(row, LV_SYMBOL_IMAGE "  Brightness");
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    valueLabel = sectionLabel(row, "");
    lv_obj_set_style_text_color(valueLabel, th.text, 0);
    lv_obj_align(valueLabel, LV_ALIGN_RIGHT_MID, 0, 0);

    slider = lv_slider_create(box);
    lv_obj_set_size(slider, lv_pct(100), 32);
    lv_obj_set_style_margin_bottom(slider, 8, 0);
    lv_slider_set_range(slider, 10, 255);
    lv_slider_set_value(slider, app::config.display.brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, sliderChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(slider, sliderReleased, LV_EVENT_RELEASED, nullptr);
    updateLabel();

    lv_obj_t* sleepBtn = lv_button_create(box);
    lv_obj_set_size(sleepBtn, lv_pct(100), 72);
    lv_obj_add_event_cb(sleepBtn, sleepClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* sleepLabel = lv_label_create(sleepBtn);
    lv_label_set_text(sleepLabel, LV_SYMBOL_POWER "  Screen off");
    lv_obj_set_style_text_font(sleepLabel, &lv_font_montserrat_28, 0);
    lv_obj_center(sleepLabel);

    sectionLabel(box, LV_SYMBOL_ENVELOPE "  Last message");
    messageLabel = sectionLabel(box, "");
    lv_obj_set_width(messageLabel, lv_pct(100));
    lv_obj_set_height(messageLabel, 66);
    lv_label_set_long_mode(messageLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(messageLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(messageLabel, th.text, 0);
    lv_obj_set_flex_grow(messageLabel, 1);
    updateMessage();

    lv_obj_t* version = sectionLabel(box, "");
    lv_obj_set_style_text_font(version, &lv_font_montserrat_14, 0);
    lv_label_set_text(version, "Version " FIRMWARE_VERSION "  (" __DATE__ ")");
}

void settingsPageTick() {
    // Brightness may also change via web interface or MQTT.
    if (slider && !lv_obj_has_state(slider, LV_STATE_PRESSED) &&
        lv_slider_get_value(slider) != app::config.display.brightness) {
        lv_slider_set_value(slider, app::config.display.brightness, LV_ANIM_OFF);
        updateLabel();
    }
    if (messageLabel && shownRevision != messageRevision()) {
        updateMessage();
    }
}

void settingsPageReset() {
    slider = nullptr;
    valueLabel = nullptr;
    messageLabel = nullptr;
}

}  // namespace ui
