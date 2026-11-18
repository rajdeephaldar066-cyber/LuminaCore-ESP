# Hardware and Safety Notes

This document records the hardware assumptions made by the current LuminaCore ESP firmware. Treat the pin map and component values as a design reference, then verify them against the physical lamp before applying battery power.

## Pin assignment

| Function | ESP32 GPIO | Configuration |
|---|---:|---|
| APDS-9930 SDA | 21 | I2C |
| APDS-9930 SCL | 22 | I2C |
| PIR output | 27 | Digital input |
| LED ring, 16 pixels | 25 | WS2812B, GRB |
| LED strip, 60 pixels | 26 | WS2812B, GRB |
| Battery ADC | 34 | ADC input through divider |
| Override / deep-sleep wake | 13 | Active LOW, ext0 |
| Mode button | 14 | Active LOW |
| Brightness Up | 32 | Active LOW |
| Brightness Down | 33 | Active LOW |
| Emergency button | 15 | Active LOW; strapping-pin consideration |

The source configures 76 LEDs total. Confirm the physical pixel count, data direction, common ground, and power injection points.

## Battery measurement

The firmware assumes a 2S Li-ion pack and a nominal 22k/10k divider. The conversion uses `analogReadMilliVolts()`, `VOLTAGE_DIVIDER_RATIO = 3.20`, and `BATTERY_VOLTAGE_CALIBRATION = 1.000`.

Measure the actual divider resistors and compare the reported battery voltage with a multimeter at several charge levels. Adjust the ratio and calibration multiplier using measured results. Confirm that the ADC pin voltage stays within the ESP32 board's permitted range under maximum pack voltage.

The present firmware thresholds are 6.8 V for boot floor, 6.6 V for warning, and 6.2 V for hard cutoff. They are provisional until verified against the exact cell chemistry and manufacturer limits, BMS behavior, temperature range, and loaded voltage sag.

The hard cutoff can shut down the device even while Emergency mode is active. This protects against excessive battery discharge, but it can silence the emergency light. The trade-off remains in the firmware and must be understood before using the device in any real safety-critical context.

## LED supply and thermal behavior

The FastLED current setting is an estimate used by the library to scale LED output. It is not a fuse or a hardware current limiter. The configured estimate is 4,000 mA at 5 V. Measure actual full-white current with both zones active and verify the power supply, converter, battery, BMS, wiring, connectors, and fuse ratings.

Check the 5 V rail for voltage drop and observe converter, wire, connector, and LED temperatures during sustained operation. Do not infer safe operating current from the FastLED setting alone.

## ESP32 boot and wake behavior

GPIO15 is a strapping pin on common ESP32 modules. Test cold boot with the Emergency button pressed and released; the external circuit must not force an unintended strap level during reset.

GPIO13 is configured for active-low ext0 deep-sleep wake. Verify RTC pull-up behavior, button wiring, wake polarity, and release behavior on the exact board used in the lamp.

## Sensor behavior

The APDS-9930 is polled only after PIR motion while the system is in Sleep. A reading at or below 15 lux allows Night Assist to start. The firmware does not continuously adjust lamp brightness from lux readings.

If initialization fails or repeated lux reads fail, the current fallback reports 0 lux, effectively assuming darkness. This favors lighting availability but can cause nuisance activation. The PIR is a motion sensor; it does not identify falls.

## General electrical handling

Use a regulated, current-limited bench supply during bring-up. Add a properly rated fuse and use a suitable BMS for the battery pack. Confirm polarity before connection, keep grounds common where required, and avoid connecting or disconnecting LED power wiring while energized.
