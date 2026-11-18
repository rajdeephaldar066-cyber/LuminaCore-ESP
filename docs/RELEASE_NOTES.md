# LuminaCore ESP - Release Notes

## V5.0

V5.0 is the current firmware line for the LuminaCore ESP lamp.

### Firmware behavior

- Sleep, Night Assist, Task Active, Emergency, and Low Power Alert states are retained.
- Night Assist uses PIR motion followed by an APDS-9930 ambient-light threshold check at 15 lux.
- Task Active drives both LED zones in warm white with button-controlled brightness.
- Emergency drives both zones red and is guarded from the Override and Mode buttons.
- Battery voltage monitoring includes boot, warning, recovery, and hard-cutoff thresholds.

### Reliability updates

- Battery measurement uses ESP32 calibrated millivolt readings with explicit divider and calibration constants.
- Watchdog initialization and loop-task subscription results are checked before watchdog resets are sent.
- I2C transactions use a bounded timeout that is restored after bus recovery.
- I2C retry backoff saturates at its configured ceiling.
- LED output remains governed by a 20 ms frame interval and a smoothed brightness ramp.

### Documentation updates

- Project description now reflects the implemented assistive-lamp behavior.
- Hardware assumptions and electrical verification requirements are documented separately.
- Validation procedures and normal user operation are documented without checkbox lists.
- Unsupported claims about fall detection, clinical fall prevention, continuous adaptive brightness, or circadian-health outcomes are avoided.

### Validation still required

Compilation must be confirmed with the exact ESP32 Arduino core and library versions used by the physical build. Battery voltage must be calibrated against a multimeter. LED current, supply droop, thermal performance, GPIO15 boot behavior, and GPIO13 wake behavior require bench verification.
