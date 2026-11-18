# LuminaCore ESP - User Guide

## Starting the lamp

Connect the ESP32 to its regulated 5 V supply or the correctly protected battery power system. On startup, the firmware measures the battery voltage. When it is below the configured boot floor, the lamp signals the low-voltage condition and enters deep sleep.

The normal idle state is Sleep, with both LED zones off.

## Automatic Night Assist

In Sleep, the PIR sensor watches for motion. When motion is detected, the APDS-9930 checks ambient light. If the reading is 15 lux or lower, the Zone 1 ring glows amber and the Zone 2 strip stays off.

Motion refreshes the three-minute hold period. When no new motion is detected before the period expires, the lamp returns to Sleep.

If the lux sensor is unavailable, the firmware assumes darkness. This can make Night Assist activate even when the room is bright.

## Task lighting

Press Mode to enter Task Active from Night Assist. Both LED zones illuminate warm white. Use Brightness Up and Brightness Down to change the output.

Press Mode again to return to Sleep. The Override button also toggles between Task Active and Sleep during normal operation.

## Emergency lighting

Press Emergency to turn both LED zones red at full output. Override and Mode do not cancel Emergency. Press Emergency again to return to Sleep.

A hard low-battery cutoff can still shut down the lamp while Emergency is active. The user should not assume that the signal will remain available after the pack reaches the cutoff threshold.

## Battery and sensor indicators

When the battery enters its warning range, the amber ring pulses. After the voltage recovers above the recovery threshold for the required samples, the firmware restores the state that was active before the warning.

Sensor failure is logged over Serial at 115200 baud. The firmware attempts I2C bus recovery and retries sensor initialization with increasing delays.

## Important use limitations

LuminaCore is an assistive lighting prototype. It is not a medical device, fall detector, or substitute for appropriate nighttime safety measures. PIR reports motion, not the cause or type of movement. The ambient-light sensor is used for threshold activation, not for continuously adjusting brightness.
