# Cuckoo Pi Monitor Gateway

Cuckoo Pi Monitor Gateway is an ESP32-S3 based display gateway for monitoring fermentation devices. It builds on [GravityMon Gateway](https://github.com/mp-se/gravitymon-gateway); thanks to Magnus Persson and the upstream contributors.

The gateway displays hydrometer readings and Cuckoo Pi temperature-controller information, provides a web configuration interface, and supports audible and on-screen alarm indications. Cuckoo Pi monitoring uses MQTT to subscribe to device status; the gateway does not publish controller commands through this integration. MQTT connection details and device IDs are not preconfigured: enter them in the settings page.

## Supported hardware

The following are firmware targets, not a complete bill of materials. Display dimensions and controllers are the values configured in this project; verify the board revision before selecting firmware.

| Target | Main hardware | Display configuration | PlatformIO environment |
| --- | --- | --- | --- |
| GMT1.56 (GMT154-06) | ESP32-S3 N16R8 | ST7789, 240 × 240 | `gmt154-06` |
| Waveshare 2.8 | Waveshare ESP32-S3 touch display board | ST7789, 240 × 320; CST3530 touch support | `gateway32s3wave-tft-sd` |
| Waveshare 3.5 | Waveshare ESP32-S3-Touch-LCD-3.5, SKU 30733 | 320 × 480; FT6336 touch controller | `gateway35s3wave-tft-sd` |

### 3D-printable housings

The [Waveshare 2.8 and 3.5 housing project](hardware/waveshare-housing-2.8-3.5.3mf) is a Bambu Studio 3MF file containing two plates: **Waveshare 2.8 Imperial nut** and **Waveshare 3.5 Imperial nut**.

## Firmware

Download the matching image from the [releases page](https://github.com/michundong3211-hub/cuckoo-pi-monitor-gateway/releases). Each file is an application image rather than a complete flash image. Before flashing, verify the board, flash layout and update method, and back up the device. Do not interchange images between targets.

## Build

In `web-ui`, run `npm ci` and `npm run build`, then copy `dist/assets/index.js.gz` and `style.css.gz` to `firmware/html/app.js.gz` and `app.css.gz`. In `firmware`, build the desired environment with `pio run -e <environment>`.

## Verification

The user reported successful device testing. The test firmware build and host test results are recorded in [RELEASE_NOTES.md](RELEASE_NOTES.md); that report does not establish separate acceptance of every feature.

## Copyright and licenses

This project is derived from GravityMon Gateway. `firmware/LICENSE` retains the historical MIT license and copyright notice; `web-ui/LICENSE` is GPLv3. Third-party components and fonts have their own licenses and some provenance and distribution requirements remain under review. This repository must not be described as wholly MIT licensed. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
