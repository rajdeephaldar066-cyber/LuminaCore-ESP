/*
 ====================================================================================
  LUMINACORE ESP — FIRMWARE V4.9
  Target Architecture: ESP32 Dev Board (Dual-Core, 3.3V Logic)
  Application: Autonomous Circadian Assistive Lighting & Fall Prevention (ATL Marathon)

  Base: V4.8 (full build WITH battery monitoring / voltage divider on GPIO34).

  Patch Notes (V4.9):
    1. FastLED power cap fixed. setMaxPowerInVoltsAndMilliamps() takes VOLTS as a
       uint8_t. Earlier versions passed 5000 (millivolts), which truncates to 136,
       making the "1.5A cap" a ~204W budget that never engaged. Now passes 5.
       NOTE: with the cap actually working, full warm white (~2.8A by FastLED's
       model) is limited to roughly brightness 135/255, so the top 2-3 TASK_ACTIVE
       brightness steps will look identical. Tune the steps or the cap on the bench.
    2. Gamma floor: the lowest button step used to map to gamma output 0 (LEDs off
       while still in TASK_ACTIVE). Output is now floored at GAMMA_OUTPUT_FLOOR.
    3. Watchdog + CPU usage:
         - yield() replaced by delay(1) so the idle task can actually run (yield()
           only hands the CPU to equal/higher priority tasks).
         - Task watchdog watches ONLY the loop task (.idle_core_mask = 0). Subscribing
           core 1's idle task can trip falsely because loop() never blocks.
         - Armed right after Serial.begin() so sensor-init hangs are covered too.
         - Portable across Arduino-ESP32 core 2.x and 3.x.
    4. Dirty flag: FastLED.show() is skipped while in STATE_SLEEP once an all-black
       frame has been pushed (previously pushed black frames at 50Hz forever).
    5. Overflow-safe millis() comparison for the I2C retry timer.
    Small extras: brightness is now rounded instead of truncated when converting the
    ramp level to FastLED's 0-255 scale (so a target like the gamma floor is actually
    reached), and the watchdog is fed inside the boot-floor flash loop.

  Carried over from V4.8: state-preserving low-power recovery, Emergency cannot be
  cancelled by Override/Mode, I2C backoff reaches 10s, 50 FPS frame governor,
  Emergency brightness snap, portable ADC attenuation macro.

  STILL OPEN (decisions/tuning, not bugs):
    - Hard voltage cutoff still overrides STATE_EMERGENCY (see checkPowerSafety()).
    - STATE_SLEEP blanks the LEDs immediately, so the ramp-down is not visible.
      If a visible fade-out to SLEEP is wanted, keep rendering the last active
      colors until the ramp reaches zero (and revisit the dirty flag in
      applyLighting() accordingly).
 ====================================================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <FastLED.h>
#include <APDS9930.h>
#include <driver/adc.h>
#include <esp_task_wdt.h>
#include "driver/rtc_io.h"

// ==================================================================================
// 1. HARDWARE MAPPING & CONSTANTS
// ==================================================================================
#define PIN_I2C_SDA          21
#define PIN_I2C_SCL          22
#define PIN_PIR              27
#define PIN_ZONE1_RING       25
#define PIN_ZONE2_STRIP      26
#define PIN_BATTERY_ADC      34

#define PIN_BTN_OVERRIDE     13 // Wakeup Pin (ext0 / RTC Domain)
#define PIN_BTN_MODE         14
#define PIN_BTN_UP           32
#define PIN_BTN_DOWN         33
#define PIN_BTN_EMERGENCY    15 // NOTE: boot-strapping pin; doesn't break boot but
                                 // can suppress ROM boot log if held at power-on.

#define NUM_LEDS_ZONE1       16
#define NUM_LEDS_ZONE2       60

#define MAX_CURRENT_MA       4000  // Current cap (now actually enforced, see notes)
#define SYSTEM_VOLTAGE_V     5     // VOLTS, not millivolts (FastLED takes a uint8_t)

#define WDT_TIMEOUT_SECONDS  3

// Portable ADC attenuation constant (core 3.x renamed ADC_11db to ADC_ATTEN_DB_12)
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  #define BATTERY_ADC_ATTEN ADC_ATTEN_DB_12
#else
  #define BATTERY_ADC_ATTEN ADC_11db
#endif

// Safety Voltage Guardrails (2S Li-ion Stack) — values to be confirmed on bench
const float V_BOOT_FLOOR     = 6.8f;
const float V_PRE_WARN       = 6.6f;
const float V_HARD_CUTOFF    = 6.2f;
const float ADC_REF_VOLTAGE  = 3.3f;
const float ADC_RESOLUTION   = 4095.0f;
const float VOLTAGE_DIVIDER_RATIO = 3.20f; // 22k / 10k divider network

// Timing & Frame Rate Constants
const unsigned long NIGHT_ASSIST_HOLD_MS = 180000; // 3-minute hold timer
const unsigned long DEBOUNCE_DELAY_MS    = 50;     // Button debounce threshold
const unsigned long I2C_POLL_INTERVAL_MS = 100;    // Lux throttle interval
const unsigned long FRAME_INTERVAL_MS    = 20;     // 50 FPS lock (1000ms / 50)
const float LUX_NIGHT_THRESHOLD          = 15.0f;

// Lowest allowed gamma output in TASK_ACTIVE (keeps the lowest step visibly lit)
const uint8_t GAMMA_OUTPUT_FLOOR         = 6;

// ==================================================================================
// 2. ENUMS & GLOBAL STATE STRUCTS
// ==================================================================================
enum SystemState {
  STATE_SLEEP,
  STATE_NIGHT_ASSIST,
  STATE_TASK_ACTIVE,
  STATE_EMERGENCY,
  STATE_LOW_POWER_ALERT
};

enum SensorHealthState {
  SENSOR_OK,
  SENSOR_RETRY_WAIT
};

SystemState currentState          = STATE_SLEEP;
SystemState preAlertState         = STATE_SLEEP; // State interrupted by LOW_POWER_ALERT
SensorHealthState sensorState     = SENSOR_OK;

CRGB zone1[NUM_LEDS_ZONE1];
CRGB zone2[NUM_LEDS_ZONE2];

APDS9930 apds = APDS9930();

uint8_t gamma28Table[256];

// Brightness & Ramping Control
uint8_t userTargetBrightnessPWM   = 180;
float currentRampLevel            = 0.0f;
float targetRampLevel             = 0.0f;

// Dirty flag: true once an all-black SLEEP frame has been pushed to the strips
bool lastFrameWasBlack            = false;

// Spectrum Profiles (Zero Blue for Circadian Safety)
const CRGB COLOR_CIRCADIAN_AMBER = CRGB(255, 100, 0);
const CRGB COLOR_TASK_WARM       = CRGB(255, 214, 170);
const CRGB COLOR_EMERGENCY       = CRGB(255, 0, 0);

// Tracking Variables
unsigned long nightAssistStartTime      = 0;
unsigned long lastI2CPollTime           = 0;
unsigned long lastBatteryCheckTime      = 0;
unsigned long nextI2CRetryTime          = 0;
unsigned long lastFrameTime             = 0;

uint8_t i2cFailCount                    = 0;
uint8_t i2cRetryCount                   = 0;
uint8_t lowBatConsecutiveCount          = 0;
uint8_t hardCutoffConsecutiveCount      = 0;
uint8_t batRecoveryConsecutiveCount     = 0;

struct Button {
  const uint8_t pin;
  bool lastState;
  bool debouncedState;
  unsigned long lastDebounceTime;
};

Button btnOverride  = {PIN_BTN_OVERRIDE, HIGH, HIGH, 0};
Button btnMode      = {PIN_BTN_MODE,     HIGH, HIGH, 0};
Button btnUp        = {PIN_BTN_UP,       HIGH, HIGH, 0};
Button btnDown      = {PIN_BTN_DOWN,     HIGH, HIGH, 0};
Button btnEmergency = {PIN_BTN_EMERGENCY,HIGH, HIGH, 0};

// ==================================================================================
// 3. FUNCTION PROTOTYPES
// ==================================================================================
void initWatchdog();
void initGamma28Table();
float readBatteryVoltage();
void checkPowerSafety();
void transitionTo(SystemState newState);
void enterDeepSleepWithWakeup();
bool recoverI2CBusOpenDrain();
float readAmbientLux();
void updateButtons();
void processStateMachine();
void applyLighting();

// ==================================================================================
// 4. WATCHDOG (portable across Arduino-ESP32 core 2.x and 3.x)
// ==================================================================================
// Watches ONLY the loop task. idle_core_mask = 0 on core 3.x: subscribing core 1's
// idle task can trip falsely because loop() never blocks for long. On core 2.x the
// idle-task subscriptions follow the core's sdkconfig defaults.
void initWatchdog() {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  esp_task_wdt_config_t twdt_config = {
    .timeout_ms = WDT_TIMEOUT_SECONDS * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_err_t err = esp_task_wdt_reconfigure(&twdt_config);
  if (err == ESP_ERR_INVALID_STATE) {
    err = esp_task_wdt_init(&twdt_config); // nothing was running yet: start fresh
  }
#else
  esp_err_t err = esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
#endif

  if (err != ESP_OK) {
    Serial.println(F("[WARN] Task watchdog setup failed. Running without it."));
  }
  esp_task_wdt_add(NULL); // subscribe the loop task (the task running setup()/loop())
}

// ==================================================================================
// 5. SETUP
// ==================================================================================
void setup() {
  Serial.begin(115200);

  // Arm the watchdog early so sensor-init hangs are covered too
  initWatchdog();

  // Portable attenuation symbol (see BATTERY_ADC_ATTEN definition above)
  analogSetPinAttenuation(PIN_BATTERY_ADC, BATTERY_ADC_ATTEN);
  analogReadResolution(12);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_BATTERY_ADC, INPUT);

  pinMode(PIN_BTN_OVERRIDE, INPUT_PULLUP);
  pinMode(PIN_BTN_MODE, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_EMERGENCY, INPUT_PULLUP);

  initGamma28Table();

  FastLED.addLeds<WS2812B, PIN_ZONE1_RING, GRB>(zone1, NUM_LEDS_ZONE1);
  FastLED.addLeds<WS2812B, PIN_ZONE2_STRIP, GRB>(zone2, NUM_LEDS_ZONE2);
  FastLED.setMaxPowerInVoltsAndMilliamps(SYSTEM_VOLTAGE_V, MAX_CURRENT_MA);
  FastLED.clear(true);

  // Boot-Floor Voltage Guard
  float bootVoltage = readBatteryVoltage();
  Serial.print(F("[POWER] Boot Voltage: "));
  Serial.print(bootVoltage);
  Serial.println(F(" V"));

  if (bootVoltage < V_BOOT_FLOOR) {
    Serial.println(F("[SAFETY] Voltage below boot floor. Shutting down."));
    for (int i = 0; i < 3; i++) {
      esp_task_wdt_reset(); // keep the watchdog fed during this intentional delay
      fill_solid(zone1, NUM_LEDS_ZONE1, COLOR_CIRCADIAN_AMBER);
      FastLED.show();
      delay(150);
      FastLED.clear(true);
      delay(150);
    }
    enterDeepSleepWithWakeup();
  }

  if (apds.init()) {
    apds.enableLightSensor(false);
    sensorState = SENSOR_OK;
    Serial.println(F("[SENSOR] APDS-9930 Sensor Online."));
  } else {
    Serial.println(F("[ERROR] APDS-9930 Init Failed. Scheduling Retry."));
    sensorState = SENSOR_RETRY_WAIT;
    nextI2CRetryTime = millis() + 500;
  }

  transitionTo(STATE_SLEEP);
  Serial.println(F("[SYSTEM] LuminaCore ESP Firmware V4.9 Ready."));
}

// ==================================================================================
// 6. MAIN LOOP
// ==================================================================================
void loop() {
  esp_task_wdt_reset(); // feed the watchdog at the top of every iteration

  unsigned long currentMillis = millis();

  // 1. Non-Blocking I2C Sensor Recovery with True Exponential Backoff (up to 10s).
  //    Overflow-safe signed comparison (correct across the 49.7-day millis() wrap).
  if (sensorState == SENSOR_RETRY_WAIT && (int32_t)(currentMillis - nextI2CRetryTime) >= 0) {
    if (recoverI2CBusOpenDrain() && apds.init()) {
      apds.enableLightSensor(false);
      sensorState = SENSOR_OK;
      i2cFailCount = 0;
      i2cRetryCount = 0;
      Serial.println(F("[I2C] Bus & Sensor Recovered Successfully."));
    } else {
      i2cRetryCount++;
      // Shift ceiling of 7 reaches the full 10000ms cap (100<<7=12800, clamped).
      unsigned long backoffMs = min(10000UL, 100UL << min(i2cRetryCount, (uint8_t)7));
      nextI2CRetryTime = currentMillis + backoffMs;
      Serial.print(F("[I2C] Retry failed. Next attempt in "));
      Serial.print(backoffMs);
      Serial.println(F(" ms."));
    }
  }

  // 2. Power Safety Monitoring (1000ms Interval)
  if (currentMillis - lastBatteryCheckTime >= 1000) {
    lastBatteryCheckTime = currentMillis;
    checkPowerSafety();
  }

  // 3. User Input Debouncing
  updateButtons();

  // 4. Core State Engine
  processStateMachine();

  // 5. FastLED Output Ramping (Throttled to 50 FPS / 20ms)
  if (currentMillis - lastFrameTime >= FRAME_INTERVAL_MS) {
    lastFrameTime = currentMillis;
    applyLighting();
  }

  // 1ms pause lets the idle task run (yield() only switches to equal/higher
  // priority tasks, so it never let the idle task execute) and cuts the 100% CPU
  // spin. Imperceptible against the 50ms debounce and 20ms frame interval.
  delay(1);
}

// ==================================================================================
// 7. CENTRAL STATE ENGINE & RTC DEEP SLEEP
// ==================================================================================
void transitionTo(SystemState newState) {
  if (currentState == newState) return;

  Serial.print(F("[STATE] Transition: "));
  Serial.print(currentState);
  Serial.print(F(" -> "));
  Serial.println(newState);

  currentState = newState;

  if (currentState == STATE_NIGHT_ASSIST) {
    nightAssistStartTime = millis();
  }

  if (currentState == STATE_EMERGENCY) {
    // A distress signal must be at full brightness immediately, not fade in
    // through the (throttled) exponential ramp filter.
    currentRampLevel = 1.0f;
  }
}

void enterDeepSleepWithWakeup() {
  FastLED.clear(true);

  // Configure RTC domain pull-up to prevent floating pin wakeup loops during sleep
  rtc_gpio_pullup_en((gpio_num_t)PIN_BTN_OVERRIDE);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_BTN_OVERRIDE);

  // Enable ext0 wakeup on Override Button (GPIO 13, Active LOW)
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_OVERRIDE, 0);

  Serial.println(F("[POWER] Deep Sleep Active. RTC Pull-up armed on GPIO 13."));
  Serial.flush();
  esp_deep_sleep_start();
}

// ==================================================================================
// 8. POWER SAFETY WITH HYSTERESIS & STATE-PRESERVING AUTO-RECOVERY
// ==================================================================================
float readBatteryVoltage() {
  uint32_t rawSum = 0;
  for (int i = 0; i < 16; i++) {
    rawSum += analogRead(PIN_BATTERY_ADC);
    delayMicroseconds(50);
  }
  float avgRaw = (float)rawSum / 16.0f;
  float pinVoltage = (avgRaw / ADC_RESOLUTION) * ADC_REF_VOLTAGE;
  return pinVoltage * VOLTAGE_DIVIDER_RATIO;
}

void checkPowerSafety() {
  float vBat = readBatteryVoltage();

  // 1. Hard Cutoff Guard (3 consecutive samples <= V_HARD_CUTOFF)
  //
  // OPEN DESIGN DECISION: this intentionally has NO STATE_EMERGENCY exemption.
  // On a genuine hard cutoff this will deep-sleep the device even if the strobe
  // is actively signaling distress. That protects the cells from over-discharge,
  // but it also silences the emergency signal at the worst possible moment.
  // Left unchanged pending an explicit team decision. A middle ground worth
  // considering: let STATE_EMERGENCY run a short, bounded grace window before
  // honoring the cutoff.
  if (vBat <= V_HARD_CUTOFF) {
    hardCutoffConsecutiveCount++;
    if (hardCutoffConsecutiveCount >= 3) {
      Serial.println(F("[CRITICAL] Low Voltage Cutoff. System Power Down."));
      enterDeepSleepWithWakeup();
    }
  } else {
    hardCutoffConsecutiveCount = 0;
  }

  // 2. Pre-Warning Low Battery Entry (3 consecutive samples <= V_PRE_WARN)
  if (vBat <= V_PRE_WARN && currentState != STATE_EMERGENCY) {
    lowBatConsecutiveCount++;
    if (lowBatConsecutiveCount >= 3 && currentState != STATE_LOW_POWER_ALERT) {
      Serial.println(F("[WARNING] Voltage Sag Detected. Entering Low Power Alert."));
      preAlertState = currentState; // remember what we're interrupting
      transitionTo(STATE_LOW_POWER_ALERT);
    }
  } else {
    lowBatConsecutiveCount = 0;
  }

  // 3. Low Power Alert Auto-Exit (Voltage restores for 3 consecutive samples).
  //    Restores whatever state was interrupted instead of forcing SLEEP.
  if (currentState == STATE_LOW_POWER_ALERT) {
    if (vBat >= (V_PRE_WARN + 0.1f)) {
      batRecoveryConsecutiveCount++;
      if (batRecoveryConsecutiveCount >= 3) {
        Serial.println(F("[POWER] Battery Voltage Restored. Resuming Previous State."));

        // Defensive fallback: preAlertState should never legitimately be
        // EMERGENCY (blocked above) or LOW_POWER_ALERT itself, but guard anyway.
        SystemState restoreTo =
            (preAlertState == STATE_EMERGENCY || preAlertState == STATE_LOW_POWER_ALERT)
                ? STATE_SLEEP
                : preAlertState;

        transitionTo(restoreTo);
        batRecoveryConsecutiveCount = 0;
      }
    } else {
      batRecoveryConsecutiveCount = 0;
    }
  }
}

// ==================================================================================
// 9. FAIL-SAFE LUX SENSING & OPEN-DRAIN RECOVERY
// ==================================================================================
float readAmbientLux() {
  if (sensorState == SENSOR_RETRY_WAIT) {
    return 0.0f; // Safe Default: Assume dark room to guarantee path illumination
  }

  float lux = 0.0f;
  if (apds.readAmbientLightLux(lux)) {
    i2cFailCount = 0;
    return lux;
  } else {
    i2cFailCount++;
    Serial.print(F("[I2C] Lux Read Error Count: "));
    Serial.println(i2cFailCount);

    if (i2cFailCount >= 3) {
      sensorState = SENSOR_RETRY_WAIT;
      nextI2CRetryTime = millis();
    }
    return 0.0f; // Fail-Safe default on drop
  }
}

bool recoverI2CBusOpenDrain() {
  Wire.end();

  pinMode(PIN_I2C_SCL, INPUT_PULLUP);
  pinMode(PIN_I2C_SDA, INPUT_PULLUP);

  // Check if SCL is held low by a hung slave (clock stretching lockup)
  if (digitalRead(PIN_I2C_SCL) == LOW) {
    return false;
  }

  pinMode(PIN_I2C_SCL, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_I2C_SCL, HIGH);

  // Toggle SCL to trigger clock pulse on hung slave SDA
  for (int i = 0; i < 9; i++) {
    digitalWrite(PIN_I2C_SCL, LOW);
    delayMicroseconds(10);
    digitalWrite(PIN_I2C_SCL, HIGH);
    delayMicroseconds(10);
    if (digitalRead(PIN_I2C_SDA) == HIGH) break;
  }

  // Generate I2C STOP condition
  pinMode(PIN_I2C_SDA, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_I2C_SDA, LOW);
  delayMicroseconds(10);
  digitalWrite(PIN_I2C_SCL, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_I2C_SDA, HIGH);
  delayMicroseconds(10);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  return (digitalRead(PIN_I2C_SDA) == HIGH && digitalRead(PIN_I2C_SCL) == HIGH);
}

// ==================================================================================
// 10. BUTTON ROUTING & DEBOUNCING
// ==================================================================================
void debounceButton(Button &btn) {
  bool reading = digitalRead(btn.pin);
  if (reading != btn.lastState) {
    btn.lastDebounceTime = millis();
  }
  if ((millis() - btn.lastDebounceTime) > DEBOUNCE_DELAY_MS) {
    if (reading != btn.debouncedState) {
      btn.debouncedState = reading;

      if (btn.debouncedState == LOW) { // Button Press Triggered
        if (btn.pin == PIN_BTN_OVERRIDE) {
          // Direct Toggle Action: TASK_ACTIVE <-> SLEEP.
          // Guarded: must not be able to silently cancel an active Emergency.
          if (currentState != STATE_EMERGENCY) {
            transitionTo(currentState == STATE_TASK_ACTIVE ? STATE_SLEEP : STATE_TASK_ACTIVE);
          }
        } else if (btn.pin == PIN_BTN_MODE) {
          // Cyclic Step Action: SLEEP -> NIGHT_ASSIST -> TASK_ACTIVE -> SLEEP.
          // Same Emergency guard as Override.
          if (currentState != STATE_EMERGENCY) {
            if (currentState == STATE_SLEEP) {
              transitionTo(STATE_NIGHT_ASSIST);
            } else if (currentState == STATE_NIGHT_ASSIST) {
              transitionTo(STATE_TASK_ACTIVE);
            } else {
              transitionTo(STATE_SLEEP);
            }
          }
        } else if (btn.pin == PIN_BTN_UP) {
          userTargetBrightnessPWM = min((int)userTargetBrightnessPWM + 25, 255);
        } else if (btn.pin == PIN_BTN_DOWN) {
          userTargetBrightnessPWM = max((int)userTargetBrightnessPWM - 25, 15);
        } else if (btn.pin == PIN_BTN_EMERGENCY) {
          // The only button permitted to toggle Emergency mode, by design.
          transitionTo(currentState == STATE_EMERGENCY ? STATE_SLEEP : STATE_EMERGENCY);
        }
      }
    }
  }
  btn.lastState = reading;
}

void updateButtons() {
  debounceButton(btnOverride);
  debounceButton(btnMode);
  debounceButton(btnUp);
  debounceButton(btnDown);
  debounceButton(btnEmergency);
}

// ==================================================================================
// 11. STATE MACHINE & RAMPING
// ==================================================================================
void processStateMachine() {
  unsigned long currentMillis = millis();

  switch (currentState) {
    case STATE_SLEEP:
      targetRampLevel = 0.0f;

      if (digitalRead(PIN_PIR) == HIGH) {
        if (currentMillis - lastI2CPollTime >= I2C_POLL_INTERVAL_MS) {
          lastI2CPollTime = currentMillis;
          float lux = readAmbientLux();
          if (lux <= LUX_NIGHT_THRESHOLD) {
            transitionTo(STATE_NIGHT_ASSIST);
          }
        }
      }
      break;

    case STATE_NIGHT_ASSIST:
      // Direct raw drive preset (~30% raw PWM) to bypass double-gamma reduction
      targetRampLevel = 76.0f / 255.0f;

      if (digitalRead(PIN_PIR) == HIGH) {
        nightAssistStartTime = currentMillis;
      }

      if (currentMillis - nightAssistStartTime >= NIGHT_ASSIST_HOLD_MS) {
        transitionTo(STATE_SLEEP);
      }
      break;

    case STATE_TASK_ACTIVE: {
      // Gamma 2.8 applied strictly to the manual slider setting, floored so the
      // lowest step (index 15 -> gamma 0) doesn't turn the strip completely off.
      uint8_t g = gamma28Table[userTargetBrightnessPWM];
      if (g < GAMMA_OUTPUT_FLOOR) g = GAMMA_OUTPUT_FLOOR;
      targetRampLevel = (float)g / 255.0f;
      break;
    }

    case STATE_EMERGENCY:
      targetRampLevel = 1.0f; // currentRampLevel is snapped to 1.0 in transitionTo()
      break;

    case STATE_LOW_POWER_ALERT:
      targetRampLevel = (50.0f + 50.0f * (0.5f * (1.0f + sin(currentMillis / 300.0f)))) / 255.0f;
      break;
  }
}

void initGamma28Table() {
  for (int i = 0; i < 256; i++) {
    gamma28Table[i] = (uint8_t)(pow((float)i / 255.0f, 2.8f) * 255.0f + 0.5f);
  }
}

void applyLighting() {
  // Smooth exponential target ramping (always runs, even when output is skipped)
  currentRampLevel += (targetRampLevel - currentRampLevel) * 0.05f;

  // Dirty flag: in STATE_SLEEP every pixel is black, so once one black frame has
  // been pushed there is nothing new to send. Skip the redundant show().
  // (Revisit this if SLEEP is ever changed to fade out visibly.)
  if (currentState == STATE_SLEEP && lastFrameWasBlack) {
    return;
  }

  // Rounded (not truncated) so targets such as the gamma floor are actually reached
  FastLED.setBrightness((uint8_t)(currentRampLevel * 255.0f + 0.5f));

  switch (currentState) {
    case STATE_SLEEP:
      fill_solid(zone1, NUM_LEDS_ZONE1, CRGB::Black);
      fill_solid(zone2, NUM_LEDS_ZONE2, CRGB::Black);
      break;

    case STATE_NIGHT_ASSIST:
      fill_solid(zone1, NUM_LEDS_ZONE1, COLOR_CIRCADIAN_AMBER);
      fill_solid(zone2, NUM_LEDS_ZONE2, CRGB::Black);
      break;

    case STATE_TASK_ACTIVE:
      fill_solid(zone1, NUM_LEDS_ZONE1, COLOR_TASK_WARM);
      fill_solid(zone2, NUM_LEDS_ZONE2, COLOR_TASK_WARM);
      break;

    case STATE_EMERGENCY:
      fill_solid(zone1, NUM_LEDS_ZONE1, COLOR_EMERGENCY);
      fill_solid(zone2, NUM_LEDS_ZONE2, COLOR_EMERGENCY);
      break;

    case STATE_LOW_POWER_ALERT:
      fill_solid(zone1, NUM_LEDS_ZONE1, COLOR_CIRCADIAN_AMBER);
      fill_solid(zone2, NUM_LEDS_ZONE2, CRGB::Black);
      break;
  }

  FastLED.show();
  lastFrameWasBlack = (currentState == STATE_SLEEP);
}
