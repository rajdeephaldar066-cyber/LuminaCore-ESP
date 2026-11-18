# Hardware, Wiring & Safety Notes

This document describes the current V5.0 firmware assumptions. Verify every item against the actual assembled lamp before applying battery power.

## Pin assignment

| Function | GPIO | Firmware configuration |
|---|---:|---|
| APDS-9930 SDA | 21 | I2C |
| APDS-9930 SCL | 22 | I2C |
| PIR output | 27 | Digital input |
| LED ring, 16 pixels | 25 | WS2812B / GRB |
| LED strip, 60 pixels | 26 | WS2812B / GRB |
| Battery ADC | 34 | ADC input |
| Override / deep-sleep wake | 13 | Active LOW, ext0 |
| Mode | 14 | Active LOW |
| Brightness Up / Down | 32 / 33 | Active LOW |
| Emergency | 15 | Active LOW; strapping pin |

Firmware LED count is **76 total (16 + 60)**. Verify physical count and power draw; do not use older documents that may state 70.

## Battery measurement

The source assumes a 2S Li-ion pack and a 22k/10k divider, represented by VOLTAGE_DIVIDER_RATIO = 3.20. V5.0 samples analogReadMilliVolts() and multiplies by the divider ratio and BATTERY_VOLTAGE_CALIBRATION.

- Measure divider resistors and pack voltage with a calibrated multimeter.
- Compare serial output with the multimeter at multiple pack voltages.
- Adjust both constants only from measured data.
- Confirm ADC pin voltage never exceeds the ESP32 board's permitted input range.
- Treat 6.8V boot, 6.6V pre-warning, and 6.2V hard cutoff as provisional until cell datasheet and BMS requirements are checked.

The hard cutoff is allowed to shut down even in Emergency mode. This can silence the signal; it is a known unresolved trade-off, not a certified safety behavior.

## LED supply and thermal safety

The FastLED current setting is a software estimate only. It cannot replace a fuse, BMS, current-limited bench supply, correctly sized wiring/connectors, or thermal checks. Measure worst-case current with both zones at full white and verify converter temperature and 5V rail stability.

## Boot and wake tests

GPIO15 is a strapping pin on common ESP32 modules. Cold boot with Emergency held and released. GPIO13 is used for ext0 wake; confirm active-low wiring and ensure the wake button is not left in a state that causes immediate repeated wakeups.

## Sensor behavior

APDS-9930 lux readings are checked only after PIR motion while in Sleep. Night Assist trigger threshold is 15 lux. Firmware does not implement continuous lux-driven brightness adjustment. If sensor initialization or repeated reads fail, it currently returns 0 lux (assume dark), favoring illumination but potentially causing nuisance activation.

PIR detects motion, not a fall. Do not describe the device as a fall detector or claim validated fall prevention.
