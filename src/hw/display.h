#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

namespace hw {

constexpr int kScreenWidth = 480;
constexpr int kScreenHeight = 480;

// ZX3D95CE01S-TR-4848: GC9503V (16-bit RGB, SPI init) + FT6336U touch.
class Display : public lgfx::LGFX_Device {
public:
    Display();

private:
    lgfx::Bus_RGB bus_;
    lgfx::Panel_GC9503 panel_;
    lgfx::Light_PWM light_;
    lgfx::Touch_FT5x06 touch_;
};

extern Display display;

bool displayBegin();
void setBacklight(uint8_t level);

}  // namespace hw
