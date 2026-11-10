# Smart Panlee ZX3D95CE01S-TR-V12 — PlatformIO Setup

## Hardware

| Component | Specification |
|---|---|
| MCU | WT32-S3-WROVER-N16R8 (ESP32-S3) |
| Flash | 16 MB |
| PSRAM | 8 MB |
| Display | 4" IPS, 480 × 480 |
| LCD controller | GC9503V |
| Display interface | 16-bit RGB + SPI initialization |
| Touch controller | FT6336U / FT5x06 (I²C) |
| Connectivity | Wi-Fi, Bluetooth LE |
| Extras | RS485, temperature/humidity sensor |

## PlatformIO Project

- **Board:** Espressif ESP32-S3-DevKitC-1
- **Board ID:** `esp32-s3-devkitc-1`
- **Framework:** Arduino or ESP-IDF
- **Platform:** Espressif 32

### platformio.ini

```ini
[env:smartpanlee]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

board_build.flash_mode = qio
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
board_build.arduino.memory_type = qio_opi

build_flags =
    -DBOARD_HAS_PSRAM

monitor_speed = 115200
```

## GPIO Mapping

| Function | GPIO |
|---|---|
| Backlight | 5 |
| Touch SDA / SCL | 15 / 6 |
| Touch interrupt | 4 |
| LCD SPI CS / CLK / MOSI | 38 / 45 / 48 |
| RGB PCLK / DE | 39 / 40 |
| RGB VSYNC / HSYNC | 41 / 42 |
| RGB D0–D7 | 45, 48, 47, 0, 21, 14, 13, 12 |
| RGB D8–D15 | 11, 16, 17, 18, 8, 3, 46, 10 |

**Note:** GPIO45 and GPIO48 are shared between LCD initialization and RGB data.

## Display Software

- **GUI:** LVGL
- **Display driver:** GC9503V
- **Touch driver:** FT6336U / FT5x06
- **Recommended BSP:** ZX3D95CE01S-TR-4848

## Resources

- [Manufacturer BSP](https://github.com/smartpanle/QMSD-ESP32-BSP)
- [TR-V12 Community Project](https://esp32-s3-86-box-project-base.sourceforge.io/)
- [openHASP TR-V12 Discussion](https://github.com/HASwitchPlate/openHASP/discussions/882)
- [Hardware Datasheet](https://fccid.io/m/5194fb37a16f487d532d400c827960427793d928b35947da01709a9c4fbe7bf8.pdf)

## Important Notes

1. PlatformIO board selection configures the ESP32-S3, not the LCD.
2. The GC9503V requires model-specific RGB/SPI initialization.
3. Start with the manufacturer's BSP.
4. ESP-IDF may provide easier BSP integration than Arduino.
5. A 3.3 V USB-to-UART adapter may be required for flashing.
6. Verify the GPIO mapping and PSRAM configuration against your exact TR-V12 hardware revision before use.