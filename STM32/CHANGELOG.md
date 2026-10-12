# STM32 Changelog

This changelog tracks firmware refactors, the reason for each change, and what
has and has not been validated. Confirm the selected firmware snapshot before
using an entry to plan hardware tests.

## 2026-10-11

### GSEquelCode4 sensor extraction (`e861215`)

- Moved ADS1256 pressure acquisition and conversion behind a dedicated sensor
  module, preserving the channel-pipeline alignment and missing-module
  safeguard.
- Moved HX711 initialization, calibration, readiness, and four-sample readings
  into a load-cell module.
- **Why:** reduce `main.cpp` responsibility while keeping the existing sensor
  behavior explicit and easier to review independently.

### GSEquelCode4 application and hardware-service extraction (`37cebb4`)

- Added `SDLogger` for SD card/file lifecycle and CSV rows.
- Added `ValveController` for valve state, servo endpoints, and launch timing.
- Added `SerialProtocol` for serial framing, command parsing, and telemetry
  formatting; simplified `main.cpp` to orchestrate the modules.
- Updated the GSEquelCode4 README with the serial compatibility notes and
  hardware validation checklist.
- **Why:** make the firmware's startup and update flow easier to follow and
  keep hardware/protocol responsibilities isolated without intentionally
  changing the wire format or launch timing.
- **Validation:** `pio run -d STM32\\GSEquelCode4` succeeded. ADC behavior,
  sensor readings, valve endpoints, SD writes, and launch timing still require
  the safe hardware checks listed in `GSEquelCode4/readme.md`.
