# V5.0 Bench & Demonstration Test Plan

Run tests on a current-limited bench supply first. Do not deliberately over-discharge Li-ion cells to test cutoff behavior.

## Preflight

- [ ] Confirm ESP32 board/module and Arduino core version.
- [ ] Confirm FastLED and APDS9930 library versions.
- [ ] Verify actual pin wiring, LED order, and total count (16 + 60 = 76).
- [ ] Inspect polarity, grounds, fuse, battery BMS, converter and connector ratings.
- [ ] Confirm GPIO15 cold-boot behavior.
- [ ] Calibrate battery voltage against a multimeter.

## Functional tests

| ID | Setup/action | Expected result |
|---|---|---|
| F01 | Power on with pack above validated boot floor | Serial reports voltage; system enters Sleep |
| F02 | Bright ambient light, trigger PIR | Night Assist remains off |
| F03 | Ambient light ≤15 lux, trigger PIR | Amber ring turns on |
| F04 | Move during Night Assist | 3-minute hold timer refreshes |
| F05 | Stop motion for 3 minutes | Returns to Sleep |
| F06 | Press Mode from Sleep | Night Assist; next press Task Active; next Sleep |
| F07 | Task Active, press Up/Down | Brightness changes within configured limits |
| F08 | Enter Emergency | Both zones red; Override/Mode cannot cancel |
| F09 | Exit Emergency with Emergency button | Returns to Sleep |
| F10 | Simulate low voltage safely using a bench supply | Low Power Alert and recovery work as expected |
| F11 | Disconnect APDS-9930 | Fail-safe assumes darkness; document nuisance activation risk |
| F12 | Restore APDS-9930 | I2C retry/recovery returns sensor to service |
| F13 | Enter deep sleep and press Override | Device wakes through GPIO13 |
| F14 | Run both zones at worst-case brightness | Measure current, voltage droop and temperature |

## Evidence to capture

For each test, record date, firmware commit, board/core/library versions, setup, ambient lux, measured battery voltage, current, temperature, result, and a short video where useful.

## Known limitations to disclose

- Lux is a motion-gated threshold decision, not continuous adaptive brightness.
- PIR detects motion; it does not detect falls.
- RGB channel values alone do not establish circadian or blue-light safety.
- Sensor failure assumes darkness and may cause nuisance illumination.
- FastLED power limiting is an estimate, not a hardware current limiter.
- Hard battery cutoff overrides Emergency mode.
- Battery thresholds and ADC calibration require hardware validation.
