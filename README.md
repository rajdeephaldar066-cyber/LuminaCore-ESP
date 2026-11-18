# LuminaCore ESP

**An ESP32-powered assistive lamp that pays attention to the room before lighting it up.** It combines ambient-light sensing and motion detection for low-level nighttime illumination, with separate task-lighting and emergency modes. The idea is simple: when the room is already bright, the lamp stays out of the way; when it is dark and someone moves, it brings up a gentle amber light.

It runs locally on the ESP32. No cloud account, no companion app, no “please sign in to turn on your lamp” situation. We have enough of those.

LuminaCore is a student-built embedded-systems project focused on practical lighting control, power monitoring, and reliable behavior with real hardware. It is not a medical device or a fall detector. The PIR sensor detects motion; calling that fall detection would be a pretty wild upgrade to give one sensor.

## What it does

- **Night Assist:** When motion is detected in Sleep and the APDS-9930 reports 15 lux or less, the 16-pixel ring turns amber. Motion refreshes the three-minute hold timer.
- **Task Active:** Both LED zones offer five calm colour presets: warm white, soft peach, muted lavender, gentle sage, and soft sky. Press Mode to cycle the colours; use Up and Down to adjust brightness. Use Override to leave Task Active.
- **Emergency:** Both zones turn red at full output. Only the Emergency button toggles this mode; the other buttons cannot silently cancel it.
- **Low Power Alert:** The ring pulses amber when the battery enters the warning range. The previous operating state is restored after voltage recovery.
- **Battery monitoring:** The ESP32 samples the divided battery voltage and applies boot, warning, and hard-cutoff thresholds.

Basically, the lamp checks whether the room actually needs light before turning on. Not revolutionary, just useful at 2 a.m. Low-key, that is the whole pitch: check the room, notice motion, light the way. We are building a lamp, not giving the ESP32 an identity crisis.

## How the light sensing works

The APDS-9930 is used as a **threshold gate**, not as a continuous auto-brightness controller. While the system is in Sleep, the firmware checks for PIR motion and then polls ambient lux at a 100 ms interval. A reading at or below 15 lux can trigger Night Assist.

If the sensor is unavailable or repeated reads fail, the current firmware assumes the room is dark. That keeps the path-lighting behavior available, but it can also cause an unwanted activation. The failure mode is intentional and should be understood when testing the lamp.

## Hardware

| Part | ESP32 pin | Notes |
|---|---:|---|
| APDS-9930 SDA | GPIO 21 | I2C |
| APDS-9930 SCL | GPIO 22 | I2C |
| PIR sensor output | GPIO 27 | Motion input |
| LED Zone 1 ring | GPIO 25 | 16 WS2812B pixels |
| LED Zone 2 strip | GPIO 26 | 60 WS2812B pixels |
| Battery voltage input | GPIO 34 | ADC through divider |
| Override / wake button | GPIO 13 | Active LOW, ext0 wake |
| Mode button | GPIO 14 | Active LOW |
| Brightness Up | GPIO 32 | Active LOW |
| Brightness Down | GPIO 33 | Active LOW |
| Emergency button | GPIO 15 | Active LOW; ESP32 strapping pin |

The firmware configures **76 addressable LEDs in total**: 16 in Zone 1 and 60 in Zone 2. Check the physical build against those counts; the code will not magically notice an extra strip because the vibes are right. Please do not make the firmware guess.

## Firmware structure

The firmware is organized around five states: Sleep, Night Assist, Task Active, Emergency, and Low Power Alert. LED output is refreshed through FastLED with a 20 ms frame interval and a smoothed brightness ramp, except that Emergency snaps to full output.

The main reliability features include a three-second task watchdog, bounded I2C transactions, sensor-bus recovery with exponential backoff, button debouncing, a gamma 2.8 brightness table, and low-voltage hysteresis.

Previous code versions from V1.0 through V4.8 exist. They are not included here at present and can be added later, if requested or required, in a separate branch.

## Build environment

The source file is `LuminaCore_ESP_V5_1.ino`.

Required software:

- Arduino IDE or PlatformIO
- ESP32 Arduino core (use the version installed on the actual device)
- FastLED 3.6 or later
- An APDS9930 library compatible with the `APDS9930.h` interface used by the sketch

Select the correct ESP32 board, connect the board over USB, compile, and upload. Keep the serial monitor at 115200 baud to view sensor, state, and battery messages.

## Calibration and safety

Battery readings are based on `analogReadMilliVolts()`, multiplied by `VOLTAGE_DIVIDER_RATIO` and `BATTERY_VOLTAGE_CALIBRATION`. The current divider assumption is 22k/10k and a ratio of 3.20. These values must be checked against the assembled resistor network and a multimeter before relying on the battery thresholds.

The configured battery thresholds are 6.8 V for the boot floor, 6.6 V for the warning, and 6.2 V for the hard cutoff. Treat them as provisional until confirmed against the specific cell datasheet, battery-management system, and loaded voltage behavior.

FastLED's 4,000 mA power setting is a software estimate, not a physical current limiter. A fuse, suitable BMS, correctly rated wiring and connectors, an adequate 5 V supply, and thermal checks are still required. Measure the actual full-white current before using the battery-powered lamp.

The hard battery cutoff currently overrides Emergency mode and can turn the lamp off while the red signal is active. GPIO15 is also a strapping pin on common ESP32 boards, so cold boot must be tested with the Emergency button both pressed and released. GPIO13 deep-sleep wake behavior should also be verified on the actual board.

## Documentation

- [Technical overview](docs/TECHNICAL_OVERVIEW.md)
- [Hardware and safety notes](docs/HARDWARE_AND_SAFETY.md)
- [Validation guide](docs/VALIDATION_GUIDE.md)
- [User guide](docs/USER_GUIDE.md)
- [Release notes](docs/RELEASE_NOTES.md)

A few things are intentionally not claimed here: continuous adaptive brightness, fall detection, clinically validated fall prevention, or circadian-health benefits. The current firmware does not implement or validate those features. No need to make the README more powerful than the actual lamp.
