# LuminaCore ESP - Validation Guide

This guide describes repeatable tests for the current firmware. Run electrical tests with a current-limited bench supply before relying on the battery-powered enclosure.

## Functional behavior

| Test | Setup and action | Expected behavior / record |
|---|---|---|
| Startup | Apply power with the pack above the verified boot floor | Serial reports battery voltage and the system enters Sleep |
| Bright-room gate | In bright ambient light, trigger the PIR | Night Assist does not activate |
| Dark-room gate | Reduce ambient light to 15 lux or below, then trigger the PIR | Amber ring activates |
| Motion hold | Move during Night Assist | Three-minute hold timer is refreshed |
| Timeout | Stop motion and allow the hold timer to expire | System returns to Sleep |
| Mode button | Press Mode from Sleep repeatedly | State sequence is Night Assist, Task Active, Sleep |
| Brightness buttons | In Task Active, press Up and Down | Warm-white output changes within configured bounds |
| Emergency entry | Press Emergency | Both zones turn red at full output |
| Emergency guard | While in Emergency, press Override and Mode | Emergency remains active |
| Emergency exit | Press Emergency again | System returns to Sleep |
| Low-power entry | Safely lower bench supply voltage through warning threshold | Amber low-power alert appears after qualifying samples |
| Voltage recovery | Raise bench supply voltage above recovery threshold | Previous operating state is restored |
| Sensor loss | Disconnect the APDS-9930 during controlled testing | Firmware assumes darkness; note any unintended activation |
| Sensor recovery | Reconnect the sensor | I2C recovery retries and sensor readings resume |
| Deep-sleep wake | Enter deep sleep and press Override | ESP32 wakes through GPIO13 |
| LED load | Run both zones at the brightest supported output | Record supply voltage, measured current, and component temperatures |

## Electrical measurements

Record pack voltage from a multimeter and from the firmware serial output at several battery charge levels. Record the divider resistor values used in the physical build. Calibrate `VOLTAGE_DIVIDER_RATIO` and `BATTERY_VOLTAGE_CALIBRATION` from those measurements.

Measure current with both LED zones showing full white. Record 5 V rail voltage at the LED input, converter temperature, connector temperature, and wiring temperature after sustained operation. Compare these measurements with actual component ratings, not just the software current estimate.

## Boot and fault tests

Test cold boot with GPIO15's Emergency button both held and released. Observe whether the board boots consistently. Verify GPIO13 deep-sleep wake with the actual button wiring and confirm that it does not immediately wake repeatedly.

Observe behavior with the APDS-9930 absent, disconnected during operation, and reconnected after an I2C fault. Note that the firmware treats failed lux reads as darkness, which can turn on Night Assist after motion.

Simulate low voltage with a bench supply rather than intentionally over-discharging Li-ion cells. Confirm the warning, recovery, and hard-cutoff behavior only under controlled conditions.

## Evidence log

For each run, record firmware commit, ESP32 board and Arduino core version, library versions, ambient lux, battery voltage, measured LED current, temperatures, test conditions, and observed result. A short video of the physical lamp is useful, but keep the measured values alongside it.
