# GUI Changelog

This changelog records ground-station changes and their rationale so GUI tests
can be planned against the behavior that changed. Version-specific setup and
operating instructions remain in each GUI version's README.

## 2026-10-11

### V7 startup connection selection

- Added browser-based startup discovery for supported STM32 Nucleo USB ports
  and the XBee serial adapter, with confirmation for a single detected device
  and explicit selection when multiple supported connections are found.
  FT4232H interfaces at the same USB location are grouped as one adapter.
- When no supported device is present, the setup screen offers simulated
  telemetry or a rescan. Simulation is visibly labeled and is never selected
  automatically after a serial disconnect.
- Deferred serial connection creation until the operator selects a mode so
  missing hardware no longer prevents the browser setup screen from starting.
- **Why:** make startup connection selection clearer while keeping simulation
  an explicit operator choice and avoiding simulated data being mistaken for
  live telemetry.
- The UI/manual connection flow still needs hardware validation with the
  actual STM32 USB and XBee devices; XBee USB detection alone does not confirm
  the remote radio link.
- Added mocked tests for device discovery and browser selection, including
  multiple devices, explicit simulation, stale choices, and natural COM-port
  ordering.
- Updated `run_gui.bat` to launch the server in a separate console and open the
  default browser to the local startup page; documented that workflow in the
  v7 README.
- If no supported device is detected, the setup screen now lists other serial
  ports as an explicit read-only option at the configured baud rate. The app
  validates the chosen port against a fresh scan and blocks valve/launch
  commands for unrecognized devices.
- Updated the README to explain the fallback option and its read-only
  restriction. The unit suite now has 33 passing tests, including generic-port
  discovery, stale-port rejection, and command blocking.
- Added explicit diagnostics when the local web port is occupied or a serial
  port cannot be opened because another application may be using it. Serial
  open failures are sent to the dashboard and remain visible in the app
  console. The v7 README now documents how to resolve these conflicts.
- Verified 36 GUI v7 unit tests pass, including web-port collision reporting
  and serial permission-denied diagnostics. This is software-only validation;
  serial hardware remains untested.

### V7 baseline and service separation (`c71c73b`)

- Created `Code/v7` as the working copy so GUI refactoring does not alter the
  `Code/v6` reference.
- Separated serial discovery, reconnect handling, and line framing into
  `serial_connection.py`; separated CSV output into `csv_logger.py`; and moved
  compact telemetry normalization and display-enable preparation into
  `telemetry_processing.py`.
- Moved valve command encoding and the telemetry-confirmed retry watchdog into
  `valve_commands.py`.
- Kept the serial protocol, UI payload shape, CSV schema, and existing retry
  policy to make these responsibility boundaries easier to test without
  intentionally changing operator behavior.

### Automated service tests

- Added standard-library `unittest` coverage for the four extracted services
  and startup connection selection.
- Verified 33 tests pass with `python -m unittest discover -s tests -v` from
  `Code/v7`. The tests use test doubles and do not validate physical serial
  hardware, the rendered UI in a browser, or end-to-end GUI/firmware operation.
