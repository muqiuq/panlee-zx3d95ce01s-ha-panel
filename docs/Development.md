# Development Notes

## Build & Flash

PlatformIO CLI is not on `PATH`; use the full path:

```sh
~/.platformio/penv/bin/pio run -e smartpanlee                       # build
~/.platformio/penv/bin/pio run -e smartpanlee -t upload -t monitor  # flash + serial monitor
```

After the first USB flash, updates can be installed via the web interface (System → Firmware update) using `.pio/build/smartpanlee/firmware.bin`, or in one step from the command line:

```sh
tools/ota.sh                                          # build only -> dist/ha-panel-<version>.bin
tools/ota.sh ha-panel-a1b2c3.local                    # build + upload + wait for reboot
PANEL_PASSWORD=secret tools/ota.sh 192.168.1.50       # with web interface password
tools/ota.sh --no-build 192.168.1.50                  # upload the last build only
```

### Flashing via USB-UART

With a plain 3.3 V adapter (no auto-reset): hold **IO0** to GND, pulse **EN** to GND, start the upload, release IO0 once writing starts, then pulse EN again to run the new firmware.

### Optional credentials

Copy `include/secrets.example.h` to `include/secrets.h` (git-ignored) to preset WiFi/MQTT defaults. Without WiFi credentials the panel opens the AP `HA-Panel-<id>`; the password is shown on the System page.

## Software Stack

| Area | Choice |
|---|---|
| Platform | `espressif32@6.8.1` (Arduino core 2.0.x), C++17 |
| Display / touch driver | LovyanGFX `^1.1.16` (`Panel_GC9503`, `Bus_RGB`, `Touch_FT5x06`, `Light_PWM`) |
| GUI | LVGL `~9.2.2` |
| LVGL config | No `lv_conf.h`; `-DLV_CONF_SKIP` plus overrides in `platformio.ini` `build_flags` |
| LVGL memory | C library `malloc` (large allocations go to PSRAM) |
| LVGL draw buffers | 2 × 480×48 px RGB565, internal RAM, partial render mode |
| MQTT | PubSubClient `^2.8` in its own FreeRTOS task (core 0), queues to/from the main loop |
| JSON / config | ArduinoJson `^7.2`, LittleFS `/config.json` with `/config.bak.json` backup |
| Web | Arduino `WebServer`, single-page UI gzipped into the firmware by `tools/embed_web.py` |

## Threading Model

- **Main loop (core 1):** WiFi state machine, web server, MQTT message dispatch, power management, LVGL.
  All LVGL calls and config changes happen here.
- **MQTT task (core 0):** connect/reconnect, subscribe, publish. Communicates only via mutex-protected queues (`src/net/mqtt_service.cpp`).
- Config changes from the web interface call `app::applyConfig()`, which rebuilds the UI (`ui::rebuild`) and reconfigures MQTT/WiFi/time as needed.
- **Watchdogs:** the main loop and the MQTT task are registered with the ESP task watchdog (30 s, panic → reboot, reset reason "task watchdog").
  If WiFi is configured and stays disconnected longer than `wifi.rebootAfterMinutes` (default 5), the panel reboots — disable via `wifi.rebootOnLoss`; suspended while a client is on the setup AP.

## Source Layout

| Path | Purpose |
|---|---|
| `src/main.cpp` | Calls `app::setup()` / `app::loop()` |
| `src/version.h` | Firmware version |
| `src/app/app.*` | Orchestration: setup, loop, MQTT commands, telemetry, HA discovery, mDNS, deferred restart |
| `src/app/config.*` | Config model, JSON (de)serialization with secret masking and clamping, LittleFS storage |
| `src/app/power.*` | Dim / sleep / wake based on LVGL inactivity |
| `src/hw/display.*` | LovyanGFX device: panel, RGB bus, backlight, touch |
| `src/hw/climate_sensor.*` | SHT20 on the touch I²C bus (I2C1, 0x40), non-blocking read every 10 s |
| `src/net/wifi_manager.*` | STA connect with AP fallback (random password) |
| `src/net/mqtt_service.*` | MQTT client task and queues |
| `src/net/web_portal.*` | REST API, OTA, background upload, captive-portal DNS |
| `src/ui/lvgl_port.*` | LVGL init, tick, flush and touch read (swallows the wake-up touch) |
| `src/ui/ui.*` | Builds/rebuilds all pages from config, dispatches MQTT messages to tiles |
| `src/ui/page_manager.*` | Page screens, background image, bottom navigation bar (buttons + swipe) |
| `src/ui/tile.*` | All tile types, tile faces and detail pages |
| `src/ui/detail.*` | Full-screen detail overlay and "Are you sure?" confirmation |
| `src/ui/message.*` | Pop-up messages (info/warning/error/alarm, answer buttons) |
| `src/ui/settings_page.*` | Built-in settings: brightness, screen off, last message, version |
| `src/ui/system_page.*` | Technical details, reboot, author |
| `src/ui/theme.*` | Color scheme from config, LVGL default theme |
| `src/ui/background.*` | Loads `/bg.bin` (480×480 RGB565) into PSRAM |
| `web/index.html` | Web interface source (plain HTML/CSS/JS) |
| `tools/embed_web.py` | Pre-build script: gzips `web/index.html` into `src/web/index_html.h` |

## REST API

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/status` | Device, WiFi, MQTT, display and last message |
| GET / PUT | `/api/config` | Read (secrets masked) / write and apply the configuration |
| GET | `/api/wifi/scan` | Scan for networks |
| POST | `/api/message` | Show a pop-up (same JSON as MQTT `cmd/message`) |
| POST | `/api/panel?action=wake\|sleep\|page&page=n` | Control the panel |
| GET / POST / DELETE | `/api/background` | Download / upload (multipart, raw RGB565) / remove the background |
| POST | `/api/ota` | Firmware update (multipart) |
| POST | `/api/reboot`, `/api/factory-reset` | Device actions |

Optional HTTP Basic Auth (user `admin`) when a web password is configured.

## Display Configuration (TR-4848)

Source: openHASP `user_setups/esp32s3/zw3d95ce01s_ar.ini`, env `panlee-zw3d95ce01s-tr-4848_16MB` (known working on this board).

| Item | Value |
|---|---|
| RGB data D0–D15 (B0–B4, G0–G5, R0–R4) | 45, 48, 47, 0, 21, 14, 13, 12, 11, 16, 17, 18, 8, 3, 46, 10 |
| DE / VSYNC / HSYNC / PCLK | 40 / 41 / 42 / 39 |
| Init SPI CS / SCLK / MOSI | 38 / 45 / 48 (shared with D0/D1, restored after init) |
| LCD reset | none (auto reset via VSYNC) |
| HSYNC pulse / back / front porch | 10 / 40 / 8, polarity 1 |
| VSYNC pulse / back / front porch | 10 / 40 / 10, polarity 1 |
| Pixel clock | 12 MHz, `pclk_active_neg = 0` |
| Backlight | GPIO 5, PWM channel 7, 20 kHz |
| Touch | FT6336U, I²C port 1, addr 0x38, SDA 15, SCL 6, INT 4, no reset pin |

## Hardware Verification Checklist

Not yet tested on the device. If something does not work, check:

1. **Black or garbled screen** — the GC9503 init sequence built into LovyanGFX may differ from the manufacturer's. Port the init commands from the QMSD BSP / openHASP (Arduino_GFX `gc9503v_type1`) if needed.
2. **Wrong colors or flicker** — toggle `pclk_active_neg`, adjust `freq_write`, or check the RGB data pin order.
3. **Backlight stays off** — GPIO 5 per schematic and openHASP comment; the datasheet states GPIO 4.
4. **Touch offset or mirrored** — adjust `offset_rotation` / `x_min`…`y_max` in `src/hw/display.cpp`.
5. **Flicker while changing brightness** — if it also flickers at a steady partial brightness, try other PWM frequencies (e.g. 1 kHz or 44 kHz); if any redraw glitches the image, it is likely PSRAM bandwidth (lower `freq_write`).
6. **PSRAM reported as 0** — check `board_build.arduino.memory_type = qio_opi` for the N16R8 module.
7. **Built-in sensor not detected** — the serial log prints all devices found on I2C1 (touch is 0x38). The sensor is assumed to be an SHT20 at 0x40 on the touch bus (SDA 15 / SCL 6); adjust `src/hw/climate_sensor.cpp` if the schematic says otherwise.

## Reference Projects

- `master-home-control` (sibling project): web interface, REST API, config storage, MQTT and OTA patterns — see [Plan.md](Plan.md) §7.
- [openHASP TR-4848 setup](https://github.com/HASwitchPlate/openHASP/blob/master/user_setups/esp32s3/zw3d95ce01s_ar.ini)
- [Manufacturer BSP](https://github.com/smartpanle/QMSD-ESP32-BSP)
