#include "tile.h"

#include <time.h>

#include "detail.h"
#include "net/mqtt_service.h"
#include "page_manager.h"
#include "theme.h"

namespace ui {

using app::TileType;

namespace {

struct IconEntry {
    const char* name;
    const char* symbol;
};

const IconEntry kIcons[] = {
    {"power", LV_SYMBOL_POWER},       {"home", LV_SYMBOL_HOME},         {"settings", LV_SYMBOL_SETTINGS},
    {"bell", LV_SYMBOL_BELL},         {"charge", LV_SYMBOL_CHARGE},     {"tint", LV_SYMBOL_TINT},
    {"audio", LV_SYMBOL_AUDIO},       {"play", LV_SYMBOL_PLAY},         {"up", LV_SYMBOL_UP},
    {"down", LV_SYMBOL_DOWN},         {"warning", LV_SYMBOL_WARNING},   {"eye", LV_SYMBOL_EYE_OPEN},
    {"eye_close", LV_SYMBOL_EYE_CLOSE}, {"wifi", LV_SYMBOL_WIFI},       {"refresh", LV_SYMBOL_REFRESH},
    {"list", LV_SYMBOL_LIST},         {"bars", LV_SYMBOL_BARS},         {"image", LV_SYMBOL_IMAGE},
    {"gps", LV_SYMBOL_GPS},           {"call", LV_SYMBOL_CALL},         {"envelope", LV_SYMBOL_ENVELOPE},
    {"ok", LV_SYMBOL_OK},             {"loop", LV_SYMBOL_LOOP},         {"shuffle", LV_SYMBOL_SHUFFLE},
    {"video", LV_SYMBOL_VIDEO},       {"battery", LV_SYMBOL_BATTERY_FULL}, {"bluetooth", LV_SYMBOL_BLUETOOTH},
    {"keyboard", LV_SYMBOL_KEYBOARD}, {"drive", LV_SYMBOL_DRIVE},       {"file", LV_SYMBOL_FILE},
    {"right", LV_SYMBOL_RIGHT},
};

const char* defaultIcon(TileType type) {
    switch (type) {
        case TileType::Thermostat: return LV_SYMBOL_HOME;
        case TileType::Switch: return LV_SYMBOL_POWER;
        case TileType::Light: return LV_SYMBOL_CHARGE;
        case TileType::Cover: return LV_SYMBOL_BARS;
        case TileType::Sensor: return LV_SYMBOL_EYE_OPEN;
        case TileType::BinarySensor: return LV_SYMBOL_BELL;
        case TileType::Scene: return LV_SYMBOL_IMAGE;
        case TileType::Button: return LV_SYMBOL_OK;
        case TileType::Media: return LV_SYMBOL_AUDIO;
        case TileType::Lock: return LV_SYMBOL_EYE_CLOSE;
        case TileType::Fan: return LV_SYMBOL_LOOP;
        case TileType::PageLink: return LV_SYMBOL_RIGHT;
        default: return "";
    }
}

const char* iconSymbol(const String& name, TileType type) {
    for (const auto& icon : kIcons) {
        if (name == icon.name) {
            return icon.symbol;
        }
    }
    return defaultIcon(type);
}

// HA statestream publishes JSON-encoded values, so strings arrive quoted.
String cleanPayload(const String& payload) {
    String s = payload;
    s.trim();
    if (s.length() >= 2 && s[0] == '"' && s[s.length() - 1] == '"') {
        s = s.substring(1, s.length() - 1);
    }
    return s;
}

bool parseNumber(const String& s, float& out) {
    if (s.isEmpty()) {
        return false;
    }
    char* end = nullptr;
    const float v = strtof(s.c_str(), &end);
    if (end == s.c_str()) {
        return false;
    }
    out = v;
    return true;
}

void setAutoFontText(lv_obj_t* label, const String& text) {
    const size_t len = text.length();
    const lv_font_t* font = len <= 6 ? &lv_font_montserrat_48 : len <= 11 ? &lv_font_montserrat_28 : &lv_font_montserrat_20;
    lv_obj_set_style_text_font(label, font, 0);
    lv_label_set_text(label, text.c_str());
}

lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

bool isControllable(TileType t) {
    switch (t) {
        case TileType::Thermostat:
        case TileType::Switch:
        case TileType::Light:
        case TileType::Cover:
        case TileType::Media:
        case TileType::Lock:
        case TileType::Fan:
        case TileType::Sensor:
        case TileType::Scene:
        case TileType::Button:
        case TileType::PageLink:
            return true;
        default:
            return false;
    }
}

}  // namespace

Tile::Tile(const app::TileConfig& cfg, lv_obj_t* parent, int x, int y, int w, int h) : cfg_(cfg) {
    if (cfg_.type != TileType::Empty) {
        build(parent, x, y, w, h);
    }
}

void Tile::build(lv_obj_t* parent, int x, int y, int w, int h) {
    const Theme& th = theme();
    obj_ = lv_obj_create(parent);
    lv_obj_set_pos(obj_, x, y);
    lv_obj_set_size(obj_, w, h);
    lv_obj_set_style_bg_color(obj_, th.tileBg, 0);
    lv_obj_set_style_bg_opa(obj_, th.tileOpa, 0);
    lv_obj_set_style_bg_opa(obj_, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(obj_, 0, 0);
    lv_obj_set_style_shadow_width(obj_, 0, 0);
    lv_obj_set_style_radius(obj_, 18, 0);
    lv_obj_set_style_pad_all(obj_, 14, 0);
    lv_obj_remove_flag(obj_, LV_OBJ_FLAG_SCROLLABLE);

    if (isControllable(cfg_.type)) {
        lv_obj_add_event_cb(obj_, clickedCb, LV_EVENT_CLICKED, this);
    } else {
        lv_obj_remove_flag(obj_, LV_OBJ_FLAG_CLICKABLE);
    }

    const int inner = w - 28;
    icon_ = makeLabel(obj_, &lv_font_montserrat_28, th.textSecondary);
    lv_obj_align(icon_, LV_ALIGN_TOP_LEFT, 0, 0);

    title_ = makeLabel(obj_, &lv_font_montserrat_20, th.textSecondary);
    lv_obj_set_width(title_, inner);
    lv_label_set_long_mode(title_, LV_LABEL_LONG_DOT);
    lv_label_set_text(title_, cfg_.title.c_str());
    lv_obj_align(title_, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    value_ = makeLabel(obj_, &lv_font_montserrat_48, th.text);
    lv_obj_set_width(value_, inner);
    lv_label_set_long_mode(value_, LV_LABEL_LONG_DOT);
    lv_obj_align(value_, LV_ALIGN_LEFT_MID, 0, -12);

    sub_ = makeLabel(obj_, &lv_font_montserrat_20, th.textSecondary);
    lv_obj_set_width(sub_, inner);
    lv_label_set_long_mode(sub_, LV_LABEL_LONG_DOT);
    lv_obj_align(sub_, LV_ALIGN_BOTTOM_LEFT, 0, -28);

    renderFace();
}

bool Tile::isOn() const {
    if (state_.isEmpty()) {
        return false;
    }
    switch (cfg_.type) {
        case TileType::Lock: return state_.equalsIgnoreCase("locked");
        case TileType::Cover: return !state_.equalsIgnoreCase("closed");
        case TileType::Media: return state_.equalsIgnoreCase("playing");
        default:
            return state_.equalsIgnoreCase(cfg_.payloadOn) || state_.equalsIgnoreCase("on") || state_ == "1" ||
                   state_.equalsIgnoreCase("true");
    }
}

lv_color_t Tile::accent() const {
    return parseColor(cfg_.color, theme().on);
}

String Tile::onOffLabel() const {
    if (state_.isEmpty()) {
        return "--";
    }
    if (isOn()) {
        return cfg_.labelOn.isEmpty() ? String("On") : cfg_.labelOn;
    }
    return cfg_.labelOff.isEmpty() ? String("Off") : cfg_.labelOff;
}

String Tile::fmt(float v) const {
    return isnan(v) ? String("--") : String(v, static_cast<unsigned int>(cfg_.decimals));
}

void Tile::renderFace() {
    if (!obj_) {
        return;
    }
    const Theme& th = theme();
    String value;
    String sub;
    bool active = false;
    bool bigSymbol = false;

    auto percent = [this]() {
        const float pct = (num_ - cfg_.min) / (cfg_.max - cfg_.min) * 100.0f;
        return String(static_cast<int>(constrain(pct, 0.0f, 100.0f))) + "%";
    };

    switch (cfg_.type) {
        case TileType::Thermostat:
            value = fmt(num2_) + "°";
            sub = "Target " + fmt(num_) + "°";
            active = !isnan(num_) && !isnan(num2_) && num_ > num2_;
            break;
        case TileType::Switch:
        case TileType::BinarySensor:
            value = onOffLabel();
            active = isOn();
            break;
        case TileType::Light:
        case TileType::Fan:
            value = onOffLabel();
            active = isOn();
            if (active && !isnan(num_)) {
                sub = percent();
            }
            break;
        case TileType::Cover:
            value = !isnan(num_) ? String(static_cast<int>(num_)) + "%" : (state_.isEmpty() ? String("--") : state_);
            sub = state_;
            active = isOn();
            break;
        case TileType::Sensor:
            value = !isnan(num_) ? fmt(num_) : (text_.isEmpty() ? String("--") : text_);
            value += cfg_.unit;
            break;
        case TileType::Media:
            value = text_.isEmpty() ? String("--") : text_;
            sub = state_;
            active = isOn();
            break;
        case TileType::Lock:
            value = state_.isEmpty() ? String("--") : (isOn() ? String("Locked") : String("Unlocked"));
            active = !state_.isEmpty() && !isOn();
            break;
        case TileType::Clock: {
            time_t now = time(nullptr);
            struct tm t;
            localtime_r(&now, &t);
            if (t.tm_year + 1900 < 2020) {
                value = "--:--";
            } else {
                char buf[32];
                strftime(buf, sizeof(buf), "%H:%M", &t);
                value = buf;
                strftime(buf, sizeof(buf), "%a %d.%m.%Y", &t);
                sub = buf;
            }
            break;
        }
        case TileType::Scene:
        case TileType::Button:
        case TileType::PageLink:
            bigSymbol = true;
            break;
        default:
            break;
    }

    const char* symbol = iconSymbol(cfg_.icon, cfg_.type);
    if (bigSymbol) {
        lv_label_set_text(icon_, "");
        lv_obj_set_style_text_font(value_, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(value_, parseColor(cfg_.color, th.accent), 0);
        lv_label_set_text(value_, symbol);
    } else {
        lv_label_set_text(icon_, symbol);
        lv_obj_set_style_text_color(icon_, active ? accent() : th.textSecondary, 0);
        if (cfg_.type == TileType::Media) {
            lv_obj_set_style_text_font(value_, &lv_font_montserrat_20, 0);
            lv_label_set_text(value_, value.c_str());
        } else {
            setAutoFontText(value_, value);
        }
        lv_obj_set_style_text_color(value_, cfg_.type == TileType::Lock && active ? th.warning : th.text, 0);
    }
    lv_label_set_text(sub_, sub.c_str());
}

void Tile::onMessage(const String& topic, const String& payload) {
    if (cfg_.type == TileType::Empty) {
        return;
    }
    const String v = cleanPayload(payload);
    bool matched = false;

    if (!cfg_.stateTopic.isEmpty() && topic == cfg_.stateTopic) {
        matched = true;
        float n;
        switch (cfg_.type) {
            case TileType::Thermostat:
                if (parseNumber(v, n)) {
                    num_ = n;
                }
                break;
            case TileType::Sensor:
                text_ = v;
                if (parseNumber(v, n)) {
                    num_ = n;
                    minSeen_ = isnan(minSeen_) ? n : min(minSeen_, n);
                    maxSeen_ = isnan(maxSeen_) ? n : max(maxSeen_, n);
                } else {
                    num_ = NAN;
                }
                break;
            default:
                state_ = v;
                break;
        }
    }
    if (!cfg_.valueTopic.isEmpty() && topic == cfg_.valueTopic) {
        matched = true;
        float n;
        switch (cfg_.type) {
            case TileType::Thermostat:
                if (parseNumber(v, n)) {
                    num2_ = n;
                }
                break;
            case TileType::Media:
                text_ = v;
                break;
            default:
                num_ = parseNumber(v, n) ? n : NAN;
                break;
        }
    }
    if (!matched) {
        return;
    }
    lastUpdateMs_ = millis();
    renderFace();
    if (isDetailOpen()) {
        renderDetail();
    }
}

void Tile::tick() {
    if (cfg_.type == TileType::Clock) {
        renderFace();
    }
    if (cfg_.type == TileType::Sensor && isDetailOpen()) {
        renderDetail();
    }
}

void Tile::collectTopics(std::vector<String>& out) const {
    if (cfg_.type == TileType::Empty) {
        return;
    }
    if (!cfg_.stateTopic.isEmpty()) {
        out.push_back(cfg_.stateTopic);
    }
    if (!cfg_.valueTopic.isEmpty()) {
        out.push_back(cfg_.valueTopic);
    }
}

void Tile::onClick() {
    switch (cfg_.type) {
        case TileType::Scene:
        case TileType::Button:
            send(cfg_.commandTopic, cfg_.payload.isEmpty() ? String("ON") : cfg_.payload,
                 "Activate " + cfg_.title + "?");
            break;
        case TileType::PageLink:
            showPage(cfg_.page);
            break;
        default:
            openDetail();
            break;
    }
}

void Tile::send(const String& topic, const String& payload, const String& question) {
    if (topic.isEmpty()) {
        return;
    }
    runConfirmed(cfg_.confirm, question.c_str(), [topic, payload]() { net::mqttPublish(topic, payload, false); });
}

void Tile::openDetail() {
    const Theme& th = theme();
    lv_obj_t* c = detailOpen(cfg_.title.isEmpty() ? app::tileTypeName(cfg_.type) : cfg_.title.c_str(), this,
                             detailClosedCb);

    dValue_ = makeLabel(c, &lv_font_montserrat_48, th.text);
    dSub_ = makeLabel(c, &lv_font_montserrat_20, th.textSecondary);
    lv_obj_set_style_text_align(dSub_, LV_TEXT_ALIGN_CENTER, 0);

    switch (cfg_.type) {
        case TileType::Thermostat:
            target_ = num_;
            addThermostatButtons(c);
            break;
        case TileType::Switch:
            addSwitch(c);
            break;
        case TileType::Light:
        case TileType::Fan:
            addSwitch(c);
            if (!cfg_.valueCommandTopic.isEmpty()) {
                addSlider(c, static_cast<int32_t>(cfg_.min), static_cast<int32_t>(cfg_.max));
            }
            break;
        case TileType::Cover: {
            lv_obj_t* row = addButtonRow(c);
            addCommandButton(row, LV_SYMBOL_UP, "OPEN");
            addCommandButton(row, LV_SYMBOL_STOP, "STOP");
            addCommandButton(row, LV_SYMBOL_DOWN, "CLOSE");
            if (!cfg_.valueCommandTopic.isEmpty()) {
                addSlider(c, 0, 100);
            }
            break;
        }
        case TileType::Lock: {
            lv_obj_t* row = addButtonRow(c);
            addCommandButton(row, "Lock", "LOCK");
            addCommandButton(row, "Unlock", "UNLOCK");
            break;
        }
        case TileType::Media: {
            lv_obj_set_width(dValue_, 448);
            lv_label_set_long_mode(dValue_, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_font(dValue_, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_align(dValue_, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_t* row = addButtonRow(c);
            addCommandButton(row, LV_SYMBOL_PREV, "PREVIOUS");
            addCommandButton(row, LV_SYMBOL_PLAY, "PLAY_PAUSE");
            addCommandButton(row, LV_SYMBOL_NEXT, "NEXT");
            if (!cfg_.valueCommandTopic.isEmpty()) {
                addSlider(c, 0, 100);
            }
            break;
        }
        default:
            break;
    }
    renderDetail();
}

void Tile::renderDetail() {
    if (!isDetailOpen()) {
        return;
    }
    String value;
    String sub;
    switch (cfg_.type) {
        case TileType::Thermostat:
            value = fmt(target_) + "°";
            sub = "Current " + fmt(num2_) + "°";
            break;
        case TileType::Sensor: {
            value = !isnan(num_) ? fmt(num_) : (text_.isEmpty() ? String("--") : text_);
            value += cfg_.unit;
            if (!isnan(minSeen_)) {
                sub = "Min " + fmt(minSeen_) + cfg_.unit + "  /  Max " + fmt(maxSeen_) + cfg_.unit + "\n";
            }
            sub += lastUpdateMs_ ? "Updated " + String((millis() - lastUpdateMs_) / 1000) + " s ago"
                                 : String("No data yet");
            break;
        }
        case TileType::Cover:
            value = !isnan(num_) ? String(static_cast<int>(num_)) + "%" : (state_.isEmpty() ? String("--") : state_);
            sub = state_;
            break;
        case TileType::Lock:
            value = state_.isEmpty() ? String("--") : (isOn() ? String("Locked") : String("Unlocked"));
            break;
        case TileType::Media:
            value = text_.isEmpty() ? String("--") : text_;
            sub = state_;
            break;
        case TileType::Light:
        case TileType::Fan:
            value = onOffLabel();
            if (!isnan(num_)) {
                sub = String(static_cast<int>(num_));
            }
            break;
        default:
            value = onOffLabel();
            break;
    }
    lv_label_set_text(dValue_, value.c_str());
    lv_label_set_text(dSub_, sub.c_str());

    if (dSwitch_ && !lv_obj_has_state(dSwitch_, LV_STATE_PRESSED)) {
        if (isOn()) {
            lv_obj_add_state(dSwitch_, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(dSwitch_, LV_STATE_CHECKED);
        }
    }
    if (dSlider_ && !lv_obj_has_state(dSlider_, LV_STATE_PRESSED) && !isnan(num_) &&
        cfg_.type != TileType::Media) {
        lv_slider_set_value(dSlider_, static_cast<int32_t>(num_), LV_ANIM_OFF);
    }
}

lv_obj_t* Tile::addSwitch(lv_obj_t* parent) {
    dSwitch_ = lv_switch_create(parent);
    lv_obj_set_size(dSwitch_, 160, 80);
    lv_obj_set_style_bg_color(dSwitch_, accent(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(dSwitch_, switchCb, LV_EVENT_VALUE_CHANGED, this);
    return dSwitch_;
}

lv_obj_t* Tile::addSlider(lv_obj_t* parent, int32_t min, int32_t max) {
    dSlider_ = lv_slider_create(parent);
    lv_obj_set_size(dSlider_, 380, 28);
    lv_obj_set_style_margin_top(dSlider_, 16, 0);
    lv_slider_set_range(dSlider_, min, max);
    lv_slider_set_value(dSlider_, (min + max) / 2, LV_ANIM_OFF);
    lv_obj_add_event_cb(dSlider_, sliderCb, LV_EVENT_RELEASED, this);
    return dSlider_;
}

lv_obj_t* Tile::addButtonRow(lv_obj_t* parent) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, 448, 100);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

void Tile::addCommandButton(lv_obj_t* row, const char* label, const char* payload) {
    lv_obj_t* btn = lv_button_create(row);
    lv_obj_set_size(btn, 120, 80);
    lv_obj_set_user_data(btn, const_cast<char*>(payload));
    lv_obj_add_event_cb(btn, commandButtonCb, LV_EVENT_CLICKED, this);
    lv_obj_t* l = lv_label_create(btn);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
    lv_obj_center(l);
}

void Tile::addThermostatButtons(lv_obj_t* parent) {
    lv_obj_t* row = addButtonRow(parent);
    for (int dir : {-1, 1}) {
        lv_obj_t* btn = lv_button_create(row);
        lv_obj_set_size(btn, 140, 80);
        lv_obj_set_user_data(btn, reinterpret_cast<void*>(static_cast<intptr_t>(dir)));
        lv_obj_add_event_cb(btn, stepCb, LV_EVENT_CLICKED, this);
        lv_obj_t* l = lv_label_create(btn);
        lv_label_set_text(l, dir < 0 ? LV_SYMBOL_MINUS : LV_SYMBOL_PLUS);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
        lv_obj_center(l);
    }
    lv_obj_t* apply = lv_button_create(parent);
    lv_obj_set_size(apply, 300, 64);
    lv_obj_add_event_cb(apply, applyTargetCb, LV_EVENT_CLICKED, this);
    lv_obj_t* l = lv_label_create(apply);
    lv_label_set_text(l, LV_SYMBOL_OK "  Set");
    lv_obj_center(l);
}

void Tile::clickedCb(lv_event_t* e) {
    static_cast<Tile*>(lv_event_get_user_data(e))->onClick();
}

void Tile::detailClosedCb(void* owner) {
    auto* t = static_cast<Tile*>(owner);
    t->dSwitch_ = nullptr;
    t->dSlider_ = nullptr;
    t->dValue_ = nullptr;
    t->dSub_ = nullptr;
}

void Tile::switchCb(lv_event_t* e) {
    auto* t = static_cast<Tile*>(lv_event_get_user_data(e));
    const bool want = lv_obj_has_state(t->dSwitch_, LV_STATE_CHECKED);
    // Show the confirmed state until HA reports the change.
    if (t->isOn()) {
        lv_obj_add_state(t->dSwitch_, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(t->dSwitch_, LV_STATE_CHECKED);
    }
    t->send(t->cfg_.commandTopic, want ? t->cfg_.payloadOn : t->cfg_.payloadOff,
            String("Turn ") + t->cfg_.title + (want ? " on?" : " off?"));
}

void Tile::sliderCb(lv_event_t* e) {
    auto* t = static_cast<Tile*>(lv_event_get_user_data(e));
    const int32_t v = lv_slider_get_value(t->dSlider_);
    t->send(t->cfg_.valueCommandTopic, String(v), "Set " + t->cfg_.title + " to " + String(v) + "?");
}

void Tile::commandButtonCb(lv_event_t* e) {
    auto* t = static_cast<Tile*>(lv_event_get_user_data(e));
    lv_obj_t* btn = lv_event_get_current_target_obj(e);
    const char* payload = static_cast<const char*>(lv_obj_get_user_data(btn));
    const char* label = lv_label_get_text(lv_obj_get_child(btn, 0));
    t->send(t->cfg_.commandTopic, payload, String(label) + "  " + t->cfg_.title + "?");
}

void Tile::stepCb(lv_event_t* e) {
    auto* t = static_cast<Tile*>(lv_event_get_user_data(e));
    const auto dir = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(lv_event_get_current_target_obj(e))));
    if (isnan(t->target_)) {
        t->target_ = isnan(t->num2_) ? (t->cfg_.min + t->cfg_.max) / 2 : t->num2_;
    }
    t->target_ = constrain(t->target_ + dir * t->cfg_.step, t->cfg_.min, t->cfg_.max);
    t->renderDetail();
}

void Tile::applyTargetCb(lv_event_t* e) {
    auto* t = static_cast<Tile*>(lv_event_get_user_data(e));
    if (isnan(t->target_)) {
        return;
    }
    const String v = t->fmt(t->target_);
    t->send(t->cfg_.commandTopic, v, "Set " + t->cfg_.title + " to " + v + "°?");
}

}  // namespace ui
