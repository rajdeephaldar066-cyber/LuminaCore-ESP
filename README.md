
# LuminaCore ESP (Version 4.9)

Autonomous Circadian Assistive Lighting & Fall Prevention System powered by an ESP32 dual-core microcontroller. Engineered for the ATL Marathon with zero cloud dependence, ultra-low latency edge execution, and hardware-level safety guardrails.

---

## 🛠️ System Architecture & V4.9 Updates
- **FastLED Power Cap Fixed (4A Budget):** `setMaxPowerInVoltsAndMilliamps()` now passes `5` volts (instead of `5000` millivolts) with `MAX_CURRENT_MA` calibrated to **4000 mA (4A)**, allowing full brightness headroom across all 70 LEDs without premature throttling.
- **Gamma Output Floor:** Added `GAMMA_OUTPUT_FLOOR = 6` to the Gamma 2.8 lookup table so the lowest manual brightness step stays visibly lit instead of blacking out the strip.
- **RTOS Watchdog & CPU Balance:** Replaced `yield()` with `delay(1)` and masked Core 1's idle task (`.idle_core_mask = 0`) to eliminate Watchdog Timer (WDT) panics.
- **Sleep-State Dirty Flag:** Skips redundant `FastLED.show()` calls in `STATE_SLEEP` once an all-black frame is pushed, conserving CPU cycles and bus traffic.
- **Overflow-Safe I2C Retries:** Uses signed `millis()` comparisons (`(int32_t)(currentMillis - nextI2CRetryTime) >= 0`) for safe operation across 49.7-day uptime rollovers.
- **SEPIC Buck-Boost Stability:** Regulates 2S Li-ion input (8.4V down to 6.2V cutoff) to a stable 5V rail via the XL6019 converter.

---

## 📌 Pin Mapping (ESP32 Dev Board)

| Component / Function | ESP32 GPIO Pin | Notes |
| :--- | :--- | :--- |
| **I2C SDA** | GPIO 21 | APDS-9930 Ambient Light / Proximity Sensor |
| **I2C SCL** | GPIO 22 | APDS-9930 Ambient Light / Proximity Sensor |
| **PIR Sensor** | GPIO 27 | Motion detection for Night Assist trigger |
| **Zone 1 & Zone 2 LEDs** | GPIO 25 / GPIO 26 | 70 Total WS2812B Addressable LEDs across dual zones |
| **Battery ADC** | GPIO 34 | 22k/10k Voltage Divider network (12-bit ADC) |
| **Override Button** | GPIO 13 | Wakeup Pin (RTC ext0 domain / Active LOW) |
| **Mode Button** | GPIO 14 | Cyclic state switcher (Internal Pull-Up) |
| **Brightness Up** | GPIO 32 | Increases manual PWM step by 25 |
| **Brightness Down** | GPIO 33 | Decreases manual PWM step by 25 |
| **Emergency Button** | GPIO 15 | Direct distress signal toggle |

---

## 🚀 Getting Started & Flashing

### Requirements
- **Arduino IDE** (or PlatformIO)
- **ESP32 Board Package** (Espressif Systems)
- **Required Libraries:**
  - `FastLED` (v3.6+)
  - `APDS9930`

### Compilation & Upload
1. Clone this repository or open `LuminaCore_ESP_V4_9.ino` in the Arduino IDE.
2. Select **Tools > Board > ESP32 Dev Module**.
3. Connect your ESP32 via USB and upload at **115200 baud**.

---

## 🛡️ Safety Guardrails
- **Hard Cutoff:** Enters RTC deep sleep if battery voltage drops to $\le 6.2\text{V}$ for 3 consecutive samples.
- **Open-Drain Bus Recovery:** Recovers hung I2C lines using a 9-clock pulse routine with exponential backoff (up to 10 seconds).
