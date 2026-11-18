# LuminaCore ESP - Technical Overview

## 1. Purpose

LuminaCore ESP is an ESP32-based assistive lamp. It combines ambient-light sensing, motion-triggered nighttime lighting, manual task illumination, emergency signalling, and battery monitoring. The firmware uses a local state machine and does not require a cloud connection.

The current sensor logic is threshold-based. It is not continuous ambient-light adaptation, fall detection, or a medical monitoring function.

## 2. Hardware architecture

The firmware targets an ESP32 development board with two WS2812B LED zones:

| Subsystem | Firmware configuration |
|---|---|
| Zone 1 | 16 WS2812B pixels on GPIO25 |
| Zone 2 | 60 WS2812B pixels on GPIO26 |
| Ambient light sensor | APDS-9930 on I2C GPIO21/GPIO22 |
| Motion input | PIR sensor on GPIO27 |
| Battery measurement | ADC GPIO34 through a nominal 22k/10k divider |
| User controls | GPIO13, GPIO14, GPIO32, GPIO33, GPIO15 |
| LED supply | 5 V regulated rail |
| Battery | 2S Li-ion pack assumption |

The two LED zones contain 76 addressable pixels in total. The pin map should be checked against the wiring of the final enclosure.

## 3. Operating states

### Sleep

The LED zones are black. The firmware checks the PIR input. When motion is present, it polls the APDS-9930 no more often than every 100 ms. Ambient light at or below 15 lux triggers Night Assist.

### Night Assist

The Zone 1 ring displays amber at a raw target level of 76/255. Zone 2 remains off. PIR motion refreshes the three-minute hold timer; when motion stops and the timer expires, the state returns to Sleep.

### Task Active

Both LED zones display the selected colour preset. Five presets are available: warm white (default), soft peach, muted lavender, gentle sage, and soft sky. Press Mode while in Task Active to cycle through them; Up and Down continue to adjust brightness. Override exits Task Active. A gamma 2.8 lookup table shapes the output, with a minimum output floor of 6. These RGB presets are ambient colour choices, not certified task-lighting spectra.

### Emergency

Both zones display red. The brightness ramp is snapped to full output when entering the state. Override and Mode inputs are ignored while Emergency is active; the Emergency button is the only normal button exit.

### Low Power Alert

Zone 1 pulses amber and Zone 2 stays off. The state interrupted by the warning is saved and restored when battery voltage has recovered for the required consecutive samples.

## 4. Sensor handling

The APDS-9930 ambient-light reading is evaluated after PIR motion in Sleep. The lux threshold is 15.0. There is no continuous lux-driven brightness adjustment.

If the sensor cannot initialize or repeated reads fail, the firmware uses 0 lux as its fallback reading. This treats the environment as dark and keeps the nighttime lighting path available, but it can cause an unwanted activation. An I2C recovery routine ends the bus, attempts open-drain clock pulses and a STOP condition, then reinitializes I2C. Retry delay increases up to ten seconds.

## 5. Battery monitoring

The firmware samples GPIO34 using `analogReadMilliVolts()` across 16 readings. The averaged pin voltage is multiplied by the nominal divider ratio of 3.20 and the calibration multiplier of 1.000.

| Threshold | Firmware response |
|---|---|
| 6.8 V boot floor | Enters deep sleep during startup when pack voltage is below the floor |
| 6.6 V pre-warning | Enters Low Power Alert after three qualifying samples |
| 6.7 V recovery | Restores the previous state after three qualifying samples |
| 6.2 V hard cutoff | Enters deep sleep after three consecutive readings at or below the threshold |

The thresholds are provisional and require verification against the cell and BMS specifications. ADC divider ratio and calibration multiplier must be checked against a multimeter.

Hard cutoff is not exempted during Emergency mode. Consequently, a low battery can shut down the red signal. This is a known behavior and safety trade-off.

## 6. Timing and reliability controls

- Main LED frame interval: 20 ms (nominal 50 FPS).
- Lux polling interval in Sleep after motion: 100 ms.
- Night Assist hold period: 180,000 ms (three minutes).
- Button debounce interval: 50 ms.
- Task watchdog timeout: three seconds when watchdog initialization and loop-task subscription succeed.
- I2C transaction timeout: 50 ms, reapplied after bus recovery.
- Sensor retry delay: exponential backoff capped at ten seconds.

The main loop feeds the watchdog only when the firmware successfully subscribed to it. A one-millisecond delay gives the RTOS idle task time to run.

## 7. LED output and power limits

FastLED is configured for a 5 V rail and a 4,000 mA software current estimate. This setting is not a physical current limiter. It does not replace a BMS, fuse, rated power converter, correctly sized wires, or thermal validation.

Night Assist uses amber, Task Active uses one of five selectable calm RGB presets, Emergency uses red, and Low Power Alert uses a pulsing amber ring. RGB values alone do not establish spectral characteristics or health effects.

## 8. Controls

| Control | GPIO | Action |
|---|---:|---|
| Override | 13 | Toggles between Task Active and Sleep; also used for ext0 wake |
| Mode | 14 | Cycles Sleep, Night Assist, Task Active |
| Brightness Up | 32 | Increases target brightness |
| Brightness Down | 33 | Decreases target brightness |
| Emergency | 15 | Enters or exits Emergency |

GPIO15 is a strapping pin on common ESP32 modules and needs cold-boot testing with the button held and released. GPIO13 wake behavior should be tested on the actual board.

## 9. Known limitations

- Lux sensing is a motion-gated threshold decision, not continuous adaptive brightness.
- Sensor failure is treated as darkness and may cause nuisance activation.
- PIR detects motion only; it does not detect falls.
- The RGB presets have not been characterized spectrally.
- The software current estimate is not a hardware current limiter.
- Hard battery cutoff overrides Emergency mode.
- Sleep blanks the LED zones immediately rather than displaying a visible fade-out.

These limitations should be reflected accurately in demonstrations and project descriptions.
