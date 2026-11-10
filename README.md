# HA Wall Panel

A wall-mounted touch remote for [Home Assistant](https://www.home-assistant.io/), built on the
**Panlee / Smart Panlee ZX3D95CE01S-TR** (ESP32-S3, 4" 480×480 IPS, capacitive touch).
Everything is configured through a built-in web interface; all communication with Home Assistant runs over **MQTT**.

> **Status:** feature-complete firmware that compiles cleanly; first hardware verification is in progress.
> See [docs/Development.md](docs/Development.md#hardware-verification-checklist) if the display does not come up.

## Features

**On the panel**

- Multiple pages, each with a **2×2 grid of configurable tiles** — switch pages with the ◀ ▶ buttons or by swiping on the bottom bar (no animations)
- **14 tile types:** thermostat, switch, light, cover/blind, sensor, binary sensor, scene/script, button, media player, lock, fan, clock/date, page link, empty
- Controlling tiles don't act on tap — they open a **detail page** with the actual controls
  (toggle, sliders, −/+ for the thermostat, open/stop/close, play/pause, …)
- Per-tile **"Are you sure?"** confirmation
- **Pop-up messages** (info / warning / error / alarm) with optional answer buttons (OK, Yes/No, OK/Cancel), sent via MQTT or the web interface
- **Dim and sleep** after inactivity; a touch wakes the screen without triggering anything
- Built-in **Settings page** (brightness, screen off, last message, version) and **System page** (WiFi, MQTT, device details, reboot)
- Customizable **color scheme** and uploadable **background image**

**Web interface**

- Status, pop-up messages, page & tile editor, appearance (presets, color pickers, live preview, background upload), display/sleep/time, WiFi (with scan) & MQTT, firmware update (OTA), config export/import, reboot, factory reset
- Setup access point with captive portal when no WiFi is configured
- Optional password protection

**Home Assistant**

- Works with plain MQTT topics — e.g. [`mqtt_statestream`](https://www.home-assistant.io/integrations/mqtt_statestream/) for states and automations for commands
- **MQTT discovery** for the panel itself: WiFi signal, uptime, display state, brightness, reboot, wake and a `notify` entity for messages
- Availability (LWT) and periodic telemetry

## Hardware

| Component | Specification |
|---|---|
| Board | Panlee ZX3D95CE01S-TR-4848 (TR-V12) |
| MCU | ESP32-S3 (WT32-S3-WROVER-N16R8), 16 MB flash, 8 MB PSRAM |
| Display | 4" IPS 480×480, GC9503V, 16-bit RGB |
| Touch | FT6336U (I²C) |

Pinout and details: [docs/Board.md](docs/Board.md).

## Getting Started

### Requirements

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- A USB-to-UART adapter (3.3 V) if the board has no USB port wired to the ESP32-S3

### Build & flash

```sh
git clone https://github.com/muqiuq/panlee-zx3d95ce01s-ha-panel.git
cd panlee-zx3d95ce01s-ha-panel

# optional: preset WiFi/MQTT credentials (file is git-ignored)
cp include/secrets.example.h include/secrets.h

pio run -t upload -t monitor
```

**Manual boot mode:** hold **IO0** to GND, briefly pull **EN** to GND, start the upload, release IO0 once writing starts, then pulse EN again.

### First setup

1. Without WiFi credentials the panel opens the access point **`HA-Panel-<id>`** — the password is shown on the panel's **System** page.
2. Connect and open any web page (captive portal) or `http://192.168.4.1/`.
3. Configure WiFi and MQTT under **Network**, then save. Afterwards the panel is reachable at `http://<hostname>.local/`.
4. Build your pages under **Pages & Tiles**, and set a web password.

Further firmware updates can be installed from the web interface (**System → Firmware update**).

## MQTT

`<base>` defaults to `panel/<device-id>` and can be changed in the web interface.

| Topic | Direction | Payload |
|---|---|---|
| `<base>/status` | panel → HA | `online` / `offline` (retained) |
| `<base>/telemetry` | panel → HA | JSON: rssi, ip, uptime, heap, brightness, display state, page |
| `<base>/cmd/reboot` · `wake` · `sleep` | HA → panel | any |
| `<base>/cmd/brightness` | HA → panel | 10–255 |
| `<base>/cmd/page` | HA → panel | page number (1-based) |
| `<base>/cmd/message` | HA → panel | plain text or JSON (see below) |
| `<base>/message/response` | panel → HA | `{"id","title","answer"}` |

Tile topics are configured individually per tile. Per-type payloads are documented in [docs/Plan.md](docs/Plan.md#11-home-assistant-integration).

### Pop-up messages

```json
{
  "title": "Front door",
  "text": "Lock the front door?",
  "level": "warning",
  "buttons": "yes_no",
  "wake": true,
  "timeout": 60,
  "id": "door1"
}
```

- `level`: `info` · `warning` · `error` · `alarm`
- `buttons`: `none` · `ok` · `yes_no` · `ok_cancel`
- `wake`: turn the screen on (default `true`)
- `timeout`: auto-close after n seconds (0 = never)
- Answers: `yes`, `no`, `ok`, `cancel`, `dismissed`, `timeout`, `replaced`

### Example: thermostat with `mqtt_statestream`

```yaml
# configuration.yaml
mqtt_statestream:
  base_topic: homeassistant
  include:
    entities:
      - climate.living_room

# automation: panel → HA
automation:
  - alias: Panel sets living room temperature
    trigger:
      - platform: mqtt
        topic: panel/living/climate/set
    action:
      - service: climate.set_temperature
        target:
          entity_id: climate.living_room
        data:
          temperature: "{{ trigger.payload | float }}"
```

Thermostat tile settings:

| Field | Value |
|---|---|
| Target temperature state topic | `homeassistant/climate/living_room/temperature` |
| Current temperature state topic | `homeassistant/climate/living_room/current_temperature` |
| Target temperature command topic | `panel/living/climate/set` |

## Project Structure

```
src/
  app/    config, power management, orchestration (MQTT commands, discovery, telemetry)
  hw/     display, touch and backlight (LovyanGFX)
  net/    WiFi manager, MQTT task, web server / REST API / OTA
  ui/     LVGL pages, tiles, detail pages, pop-ups, settings & system pages, theme
web/      web interface (embedded into the firmware at build time)
tools/    build scripts
docs/     board notes, plan, development notes
```

More: [docs/Plan.md](docs/Plan.md) (design) · [docs/Development.md](docs/Development.md) (build, architecture, REST API, hardware checklist).

## Built With

[PlatformIO](https://platformio.org/) · [Arduino-ESP32](https://github.com/espressif/arduino-esp32) ·
[LVGL](https://lvgl.io/) · [LovyanGFX](https://github.com/lovyan03/LovyanGFX) ·
[ArduinoJson](https://arduinojson.org/) · [PubSubClient](https://github.com/knolleary/pubsubclient)

Display timing and pinout based on the [openHASP](https://github.com/HASwitchPlate/openHASP) configuration for this board.

## Author

**Philipp Albrecht** · [philipp@uisa.ch](mailto:philipp@uisa.ch) · [www.uisa.ch](https://www.uisa.ch)
