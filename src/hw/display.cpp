#include "display.h"

namespace hw {

Display display;

Display::Display() {
    {
        auto cfg = panel_.config();
        cfg.memory_width = kScreenWidth;
        cfg.memory_height = kScreenHeight;
        cfg.panel_width = kScreenWidth;
        cfg.panel_height = kScreenHeight;
        cfg.offset_x = 0;
        cfg.offset_y = 0;
        panel_.config(cfg);
    }
    {
        // SPI lines for the init sequence share GPIO45/48 with RGB D0/D1.
        auto cfg = panel_.config_detail();
        cfg.pin_cs = 38;
        cfg.pin_sclk = 45;
        cfg.pin_mosi = 48;
        cfg.use_psram = 2;
        panel_.config_detail(cfg);
    }
    {
        auto cfg = bus_.config();
        cfg.panel = &panel_;
        cfg.pin_d0 = 45;   // B0
        cfg.pin_d1 = 48;   // B1
        cfg.pin_d2 = 47;   // B2
        cfg.pin_d3 = 0;    // B3
        cfg.pin_d4 = 21;   // B4
        cfg.pin_d5 = 14;   // G0
        cfg.pin_d6 = 13;   // G1
        cfg.pin_d7 = 12;   // G2
        cfg.pin_d8 = 11;   // G3
        cfg.pin_d9 = 16;   // G4
        cfg.pin_d10 = 17;  // G5
        cfg.pin_d11 = 18;  // R0
        cfg.pin_d12 = 8;   // R1
        cfg.pin_d13 = 3;   // R2
        cfg.pin_d14 = 46;  // R3
        cfg.pin_d15 = 10;  // R4

        cfg.pin_henable = 40;
        cfg.pin_vsync = 41;
        cfg.pin_hsync = 42;
        cfg.pin_pclk = 39;
        cfg.freq_write = 12000000;

        cfg.hsync_polarity = 1;
        cfg.hsync_pulse_width = 10;
        cfg.hsync_back_porch = 40;
        cfg.hsync_front_porch = 8;
        cfg.vsync_polarity = 1;
        cfg.vsync_pulse_width = 10;
        cfg.vsync_back_porch = 40;
        cfg.vsync_front_porch = 10;
        cfg.pclk_active_neg = 0;
        cfg.de_idle_high = 0;
        cfg.pclk_idle_high = 0;
        bus_.config(cfg);
    }
    panel_.setBus(&bus_);

    {
        auto cfg = light_.config();
        cfg.pin_bl = 5;
        cfg.invert = false;
        cfg.freq = 20000;
        cfg.pwm_channel = 7;
        light_.config(cfg);
    }
    panel_.light(&light_);

    {
        auto cfg = touch_.config();
        cfg.x_min = 0;
        cfg.x_max = kScreenWidth - 1;
        cfg.y_min = 0;
        cfg.y_max = kScreenHeight - 1;
        cfg.pin_int = 4;
        cfg.pin_rst = -1;
        cfg.bus_shared = false;
        cfg.offset_rotation = 0;
        cfg.i2c_port = 1;
        cfg.i2c_addr = 0x38;
        cfg.pin_sda = 15;
        cfg.pin_scl = 6;
        cfg.freq = 400000;
        touch_.config(cfg);
    }
    panel_.setTouch(&touch_);

    setPanel(&panel_);
}

bool displayBegin() {
    if (!display.init()) {
        return false;
    }
    display.fillScreen(TFT_BLACK);
    return true;
}

void setBacklight(uint8_t level) {
    display.setBrightness(level);
}

}  // namespace hw
