# LuminaCore ESP — V5.0

**Selection-stage firmware for an ambient-aware assistive lamp.** LuminaCore uses ambient-light thresholding and motion detection to provide low-level nighttime illumination, user-controlled task lighting, emergency signalling, and battery-aware operation.

> **Scope and claims:** LuminaCore is an assistive lighting prototype. It is not a medical device, fall detector, or validated fall-prevention system. Its lux sensor is used as a threshold gate for Night Assist; the firmware does not continuously modulate brightness from lux readings. No circadian-health or blue-light safety claim is made from RGB values alone.

## What's in V5.0

- Preserves the existing V4.9 feature set; advanced expansion features remain on the post-selection roadmap.
- Uses ESP32 calibrated ADC millivolt readings for battery measurement, with explicit divider and calibration constants.
- Checks watchdog initialization and loop-task subscription before feeding the watchdog.
- Bounds I2C transactions and restores the timeout after bus recovery.
- Saturates the I2C retry counter to avoid long-outage wraparound.
- Updates naming and documentation to describe current lamp behavior accurately.

## Current behavior

| State | Behavior |
|---|---|
| Sleep | LEDs off; PIR motion prompts a lux check |
| Night Assist | When motion is detected and ambient light is at or below 15 lux, the 16-LED ring glows amber; motion refreshes a 3-minute hold timer |
| Task Active | Both LED zones illuminate warm white; brightness is adjusted with Up/Down buttons |
| Emergency | Both LED zones illuminate red; only the Emergency button exits this state |
| Low Power Alert | Amber ring pulses; previous state is restored after voltage recovery |

**Sensor fallback:** if the APDS-9930 is unavailable, firmware currently assumes darkness for the Night Assist decision. This favors path illumination but can cause unwanted activation; demonstrate and disclose this behavior.

## Hardware map

| Component | ESP32 GPIO | Notes |
|---|---:|---|
| APDS-9930 SDA / SCL | 21 / 22 | I2C |
| PIR motion sensor | 27 | Digital input |
| Zone 1 ring | 25 | 16 WS2812B LEDs |
| Zone 2 strip | 26 | 60 WS2812B LEDs |
| Battery ADC | 34 | 22k/10k divider assumed |
| Override / wake button | 13 | Active LOW; RTC ext0 wake |
| Mode button | 14 | Active LOW |
| Brightness Up / Down | 32 / 33 | Active LOW |
| Emergency button | 15 | Active LOW; ESP32 strapping pin—test boot with button held |

**Firmware LED count: 76 total (16 + 60).** Verify the physical count before wiring and presentation.

## Software requirements

- Arduino IDE or PlatformIO
- ESP32 Arduino core (2.x or 3.x; compile against the exact version used on the prototype)
- FastLED 3.6 or later
- APDS9930 library compatible with the included APDS9930.h API

Open LuminaCore_ESP_V5_0.ino, select the correct ESP32 board and serial port, then compile and upload. Keep V4.9 source as a historical reference; use V5.0 for the selection-stage build.

## Calibration and safety — do before powering the complete lamp

1. **Battery ADC:** measure the battery pack with a multimeter and compare it with serial-reported voltage at several charge levels. Confirm actual divider resistor values and adjust VOLTAGE_DIVIDER_RATIO and BATTERY_VOLTAGE_CALIBRATION. Do not assume the nominal 22k/10k ratio is exact.
2. **Battery thresholds:** the 2S pack thresholds (6.8V boot floor, 6.6V warning, 6.2V hard cutoff) are provisional. Confirm cell manufacturer limits, BMS behavior, loaded voltage sag, and pack specifications.
3. **LED power:** FASTLED_ESTIMATED_CURRENT_LIMIT_MA is a FastLED software estimate, not a physical current limiter. Verify supply capacity, LED current under worst-case white, wiring, connectors, fuse, thermal behavior, and battery/BMS ratings. Current configured estimate is 4000mA at 5V; do not treat it as a proven safe operating limit.
4. **Emergency vs cutoff:** hard battery cutoff currently takes precedence over Emergency mode and can shut the lamp down during an emergency signal. This unresolved safety trade-off must be understood and evaluated.
5. **GPIO15:** it is an ESP32 strapping pin. Test cold boot with Emergency button pressed and released; change the pin if boot behavior is unreliable.
6. **Deep-sleep wake:** verify GPIO13 pull-up, button wiring, wake polarity, and release behavior to avoid repeated wake/sleep cycles.
7. **Power conversion:** verify the 2S-to-5V converter input range, output regulation, thermal performance, and transient response with the actual LED load.

Never connect or disconnect LED power wiring while energized. Use a correctly rated regulated supply for bench tests and a suitable fuse/BMS for battery operation.

## Selection-stage demonstration checklist

- [ ] Bright room + motion: Night Assist does not activate.
- [ ] Dark room (≤15 lux) + motion: amber ring activates.
- [ ] Motion during Night Assist refreshes the hold timer; no motion allows timeout after 3 minutes.
- [ ] Task Active lights both zones; Up/Down buttons change brightness.
- [ ] Emergency lights both zones red; Override and Mode do not silently cancel it.
- [ ] Low-battery alert and recovery are demonstrated safely with a bench supply or controlled setup—not by deliberately over-discharging cells.
- [ ] Sensor disconnected/reconnected behavior is observed and documented.
- [ ] Cold boot is tested with GPIO15 button states; deep-sleep wake is verified.
- [ ] Battery ADC is compared with a multimeter; LED current and temperature are measured under worst-case load.

Record short videos, measured lux values, battery voltage, current, and test conditions. These are stronger selection evidence than adding untested features.

## Repository layout

- LuminaCore_ESP_V5_0.ino — current selection-stage firmware
- LuminaCore_ESP_V4_9.ino — retained V4.9 reference
- docs/HARDWARE_AND_SAFETY.md — wiring and safety validation details
- docs/TEST_PLAN.md — repeatable bench and demonstration tests

## Roadmap

Higher-level features such as continuous adaptive brightness, display/dashboard integration, and dedicated current/power telemetry are intentionally deferred until after selection. V5.0 prioritizes a dependable, explainable demonstration of the existing lamp.
