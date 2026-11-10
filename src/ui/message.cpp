#include "message.h"

#include <ArduinoJson.h>
#include <lvgl.h>
#include <time.h>

#include "app/config.h"
#include "app/power.h"
#include "net/mqtt_service.h"
#include "theme.h"

namespace ui {

namespace {

MessageInfo last;
uint32_t revision = 0;
lv_obj_t* backdrop = nullptr;
lv_timer_t* timeoutTimer = nullptr;

String limit(const String& s, size_t maxLen) {
    return s.length() > maxLen ? s.substring(0, maxLen) : s;
}

String nowText() {
    time_t now = time(nullptr);
    if (now < 1600000000) {
        return "uptime " + String(millis() / 1000) + " s";
    }
    struct tm t;
    localtime_r(&now, &t);
    char buf[24];
    strftime(buf, sizeof(buf), "%d.%m. %H:%M", &t);
    return buf;
}

lv_color_t levelColor(const String& level) {
    const Theme& th = theme();
    if (level == "warning") {
        return th.on;
    }
    if (level == "error" || level == "alarm") {
        return th.warning;
    }
    return th.accent;
}

const char* levelSymbol(const String& level) {
    if (level == "warning") {
        return LV_SYMBOL_WARNING;
    }
    if (level == "error") {
        return LV_SYMBOL_CLOSE;
    }
    if (level == "alarm") {
        return LV_SYMBOL_BELL;
    }
    return LV_SYMBOL_ENVELOPE;
}

void closePopup() {
    if (timeoutTimer) {
        lv_timer_delete(timeoutTimer);
        timeoutTimer = nullptr;
    }
    if (backdrop) {
        lv_obj_delete_async(backdrop);
        backdrop = nullptr;
    }
}

void respond(const char* answer) {
    closePopup();
    last.answer = answer;
    last.open = false;
    ++revision;

    JsonDocument doc;
    doc["id"] = last.id;
    doc["title"] = last.title;
    doc["answer"] = answer;
    String out;
    serializeJson(doc, out);
    net::mqttPublish(app::config.mqtt.baseTopic + "/message/response", out, false);
}

void buttonCb(lv_event_t* e) {
    respond(static_cast<const char*>(lv_event_get_user_data(e)));
}

void timeoutCb(lv_timer_t*) {
    timeoutTimer = nullptr;
    respond("timeout");
}

void addButton(lv_obj_t* row, const char* label, const char* answer, bool primary) {
    lv_obj_t* btn = lv_button_create(row);
    lv_obj_set_size(btn, 160, 64);
    if (!primary) {
        lv_obj_set_style_bg_color(btn, theme().off, 0);
    }
    lv_obj_add_event_cb(btn, buttonCb, LV_EVENT_CLICKED, const_cast<char*>(answer));
    lv_obj_t* l = lv_label_create(btn);
    lv_label_set_text(l, label);
    lv_obj_center(l);
}

void showPopup(uint16_t timeoutSeconds) {
    const Theme& th = theme();
    const lv_color_t color = levelColor(last.level);

    // System layer keeps the pop-up above detail pages and confirmation dialogs.
    backdrop = lv_obj_create(lv_layer_sys());
    lv_obj_set_size(backdrop, 480, 480);
    lv_obj_set_pos(backdrop, 0, 0);
    lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_60, 0);
    lv_obj_set_style_border_width(backdrop, 0, 0);
    lv_obj_set_style_radius(backdrop, 0, 0);
    lv_obj_remove_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* box = lv_obj_create(backdrop);
    lv_obj_set_width(box, 430);
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(box, 450, 0);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, th.dialog, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, color, 0);
    lv_obj_set_style_border_width(box, last.level == "alarm" ? 6 : 3, 0);
    lv_obj_set_style_radius(box, 16, 0);
    lv_obj_set_style_pad_all(box, 20, 0);
    lv_obj_set_style_pad_row(box, 12, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* icon = lv_label_create(box);
    lv_label_set_text(icon, levelSymbol(last.level));
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(icon, color, 0);

    lv_obj_t* title = lv_label_create(box);
    lv_label_set_text(title, last.title.c_str());
    lv_obj_set_width(title, 390);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, th.text, 0);

    if (!last.text.isEmpty()) {
        lv_obj_t* text = lv_label_create(box);
        lv_label_set_text(text, last.text.c_str());
        lv_obj_set_width(text, 390);
        lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(text, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(text, th.textSecondary, 0);
    }

    lv_obj_t* row = lv_obj_create(box);
    lv_obj_set_size(row, 390, 72);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    if (last.buttons == "yes_no") {
        addButton(row, "No", "no", false);
        addButton(row, "Yes", "yes", true);
    } else if (last.buttons == "ok_cancel") {
        addButton(row, "Cancel", "cancel", false);
        addButton(row, "OK", "ok", true);
    } else if (last.buttons == "ok") {
        addButton(row, "OK", "ok", true);
    } else {
        addButton(row, LV_SYMBOL_CLOSE "  Close", "dismissed", true);
    }

    if (timeoutSeconds > 0) {
        timeoutTimer = lv_timer_create(timeoutCb, timeoutSeconds * 1000UL, nullptr);
        lv_timer_set_repeat_count(timeoutTimer, 1);
    }
}

}  // namespace

bool messageShow(const String& payload, String& error) {
    JsonDocument doc;
    const bool isJson = !deserializeJson(doc, payload) && doc.is<JsonObject>();

    MessageInfo msg;
    bool wake = true;
    uint16_t timeoutSeconds = 0;
    if (isJson) {
        msg.id = limit(doc["id"] | "", 64);
        msg.title = limit(doc["title"] | "", 64);
        msg.text = limit(doc["text"] | "", 512);
        msg.level = doc["level"] | "info";
        msg.buttons = doc["buttons"] | "none";
        wake = doc["wake"] | true;
        timeoutSeconds = constrain(doc["timeout"] | 0, 0, 3600);
    } else {
        msg.text = limit(payload, 512);
        msg.level = "info";
        msg.buttons = "none";
    }

    if (msg.level != "info" && msg.level != "warning" && msg.level != "error" && msg.level != "alarm") {
        error = "level must be info, warning, error or alarm";
        return false;
    }
    if (msg.buttons != "none" && msg.buttons != "ok" && msg.buttons != "yes_no" && msg.buttons != "ok_cancel") {
        error = "buttons must be none, ok, yes_no or ok_cancel";
        return false;
    }
    if (msg.title.isEmpty() && msg.text.isEmpty()) {
        error = "title or text required";
        return false;
    }
    if (msg.title.isEmpty()) {
        msg.title = msg.level == "warning" ? "Warning" : msg.level == "error" ? "Error" : msg.level == "alarm" ? "Alarm" : "Message";
    }

    if (last.open) {
        respond("replaced");
    }
    msg.time = nowText();
    msg.open = true;
    last = msg;
    ++revision;

    showPopup(timeoutSeconds);
    if (wake) {
        app::powerWake();
    }
    return true;
}

const MessageInfo& lastMessage() {
    return last;
}

uint32_t messageRevision() {
    return revision;
}

}  // namespace ui
