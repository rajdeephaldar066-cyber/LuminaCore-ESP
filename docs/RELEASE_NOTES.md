# LuminaCore ESP - Release Notes

## V5.1

V5.1 continues the existing LuminaCore ESP firmware line with selectable calm colour presets in Task Active.

### Change log

- Added five Task Active colour presets: warm white (default), soft peach, muted lavender, gentle sage, and soft sky.
- Mode cycles the selected colour while Task Active is running; brightness controls remain unchanged, and Override exits to Sleep.
- Updated firmware naming and project documentation for V5.1.

Previous code versions from V1.0 through V4.8 exist. They are not included here at present and can be added later, if requested or required, in a separate branch.

## Validation still required

Compile with the exact ESP32 Arduino core and library versions used by the physical build. Verify the preset sequence, brightness behavior, measured LED current, supply stability, and thermal performance on the assembled lamp. RGB values alone do not establish spectral characteristics, colour-rendering quality, or health benefits.
