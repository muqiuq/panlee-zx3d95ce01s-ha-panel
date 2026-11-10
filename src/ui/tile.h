#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include <vector>

#include "app/config.h"

namespace ui {

class Tile {
public:
    Tile(const app::TileConfig& cfg, lv_obj_t* parent, int x, int y, int w, int h);
    Tile(const Tile&) = delete;
    Tile& operator=(const Tile&) = delete;

    void onMessage(const String& topic, const String& payload);
    void tick();
    void collectTopics(std::vector<String>& out) const;

private:
    app::TileConfig cfg_;
    lv_obj_t* obj_ = nullptr;
    lv_obj_t* icon_ = nullptr;
    lv_obj_t* title_ = nullptr;
    lv_obj_t* value_ = nullptr;
    lv_obj_t* sub_ = nullptr;

    String state_;
    String text_;
    float num_ = NAN;
    float num2_ = NAN;
    float minSeen_ = NAN;
    float maxSeen_ = NAN;
    float target_ = NAN;
    uint32_t lastUpdateMs_ = 0;
    bool pending_ = false;
    uint32_t pendingSinceMs_ = 0;

    // Detail page widgets, valid while this tile's detail page is open.
    lv_obj_t* dSwitch_ = nullptr;
    lv_obj_t* dSlider_ = nullptr;
    lv_obj_t* dValue_ = nullptr;
    lv_obj_t* dSub_ = nullptr;

    bool isOn() const;
    bool isDetailOpen() const { return dValue_ != nullptr; }
    lv_color_t accent() const;
    String onOffLabel() const;
    String fmt(float v) const;

    void build(lv_obj_t* parent, int x, int y, int w, int h);
    void renderFace();
    void renderDetail();
    void onClick();
    void openDetail();
    void send(const String& topic, const String& payload, const String& question);

    lv_obj_t* addSwitch(lv_obj_t* parent);
    lv_obj_t* addSlider(lv_obj_t* parent, int32_t min, int32_t max);
    lv_obj_t* addButtonRow(lv_obj_t* parent);
    void addCommandButton(lv_obj_t* row, const char* label, const char* payload);
    void addThermostatButtons(lv_obj_t* parent);

    static void clickedCb(lv_event_t* e);
    static void detailClosedCb(void* owner);
    static void switchCb(lv_event_t* e);
    static void sliderCb(lv_event_t* e);
    static void commandButtonCb(lv_event_t* e);
    static void stepCb(lv_event_t* e);
    static void applyTargetCb(lv_event_t* e);
};

}  // namespace ui
