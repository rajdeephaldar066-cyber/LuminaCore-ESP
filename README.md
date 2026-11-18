LuminaCore ESP (Firmware V4.9)------>
Autonomous Circadian Assistive Lighting & Fall-Prevention Edge System:
LuminaCore ESP is an offline, deterministic embedded lighting controller built on the ESP32 dual-core architecture. Designed for low-vision accessibility, fall prevention, and circadian rhythm preservation, the system operates 100% locally on the metal—requiring zero cloud dependencies, Wi-Fi, or Bluetooth stacks.
Key Architectural Features:
Deterministic 50 FPS Non-Blocking State Engine: 
Manages five distinct operational states (STATE_SLEEP, STATE_NIGHT_ASSIST, STATE_TASK_ACTIVE, STATE_EMERGENCY, and STATE_LOW_POWER_ALERT) locked to a 20ms frame interval without blocking delays.
Perceptual Gamma 2.8 Dimming & Exponential Ramping: 
Uses a pre-calculated 256-step logarithmic lookup table (GAMMA_OUTPUT_FLOOR = 6) paired with a continuous per-frame smoothing filter (0.05f coefficient) to match human optical sensitivity.
Hardware-Level Open-Drain I2C Recovery: 
Features a custom low-level bus recovery routine (recoverI2CBusOpenDrain()) that drops Wire, toggles SCL up to 9 clock cycles in open-drain mode to clear hung slaves, generates a hardware STOP condition, and manages retries via an overflow-safe exponential backoff timer (capped at 10 seconds).
Active Power Guardrails & RTC Deep Sleep: 
Samples a 2S Li-ion battery pack via a 22K OHM/ 10k OHM voltage divider (GPIO 34) with 16x oversampling and hysteresis. Enforces a 6.6V pre-warning alert state (with state-preserving auto-recovery) and a 6.2V hard cutoff that arms an ext0 RTC pull-up deep sleep wakeup on GPIO 13.
Enforced Current Limiting & Bus Optimization:
Strictly enforces a 4.0A power budget at 5V via FastLED's power management and utilizes a frame dirty-flag (lastFrameWasBlack) to eliminate redundant WS2812B bus traffic during sleep states.
Cross-Core RTOS Watchdog:
Configured portably across Arduino-ESP32 Core 2.x and 3.x to monitor the main execution loop while yielding cleanly to Core 1's idle task.
Hardware Bill of Materials(BOM):
ESP32 Dev Module (Dual-Core, 3.3V Logic)-> Runs standalone without radio stacks
Ambient Light Sensor APDS-9930(I2C)-> Digital ambient lux sensing
Motion Sensor HC-SR501 / AM312 PIRDigital-> occupancy trigger (3.3V output)
Zone 1; LEDsWS2812B Addressable Ring (16 LEDs)-> Circadian night-assist & alert zone
Zone 2; LEDsWS2812B Addressable Strip (60 LEDs)-> High-output task illumination 
Zone Power Supply-> 2S Li-ion Battery Pack (6.2V – 8.4V)-Protected battery 
Pack Voltage Regulator-XL6019, Buck-Boost (SEPIC) Converter-> Regulated 5V rail for LEDs & ESP32 
VIN-Voltage Divider,22Kohm and 10Kohm resistors--> scales 8.4V max down to ~2.62V for ADC
User Controls--> 5x Momentary Tactile Pushbuttons, Internal pull-ups enabled(INPUT_PULLUP)
Dependencies & Installation:
Option 1: Arduino IDE (Recommended for Quick Flashing)
Install the ESP32 Board Package by Espressif Systems (v2.0.x or v3.0.x+) via Boards Manager.
Install the following libraries via Library Manager:
FastLED by Daniel Garcia (>= 3.6.0)
APDS9930 by Depau / Shawn Hymel (>= 1.4.3)
Instrcutions to replicate;
Select ESP32 Dev Module, set upload speed to 115200 or 921600, and flash LuminaCore_ESP_V4_9.ino.
Option 2: PlatformIO
Use the included platformio.ini configuration file to automatically fetch the Espressif32 platform and required library dependencies.
Safety & Bench Calibration Notes
Current Cap Behavior: With SYSTEM_VOLTAGE_V set to 5 and MAX_CURRENT_MA set to 4000, FastLED actively limits peak current draw. At full warm white (RGB(255, 214, 170)), all 78 LEDs will saturate the 4.0A ceiling around brightness step 225/255. Increase MAX_CURRENT_MA only if SEPIC converter and battery discharge rating support higher loads.
***Emergency Override Priority: Neither Override nor Mode buttons can cancel STATE_EMERGENCY once triggered—only a dedicated press of the Emergency button exits distress mode.
***Boot-Strapping Pin (GPIO 15): GPIO 15 is used for the Emergency button with an internal pull-up. Holding it LOW during power-on will suppress ROM serial boot messages, though normal firmware boot proceeds unaffected.
