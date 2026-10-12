# GUI Changelog

This changelog records ground-station changes and their rationale so GUI tests
can be planned against the behavior that changed. Version-specific setup and
operating instructions remain in each GUI version's README.

## 2026-10-11

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

- Added standard-library `unittest` coverage for the four extracted services.
- Verified 19 tests pass with `python -m unittest discover -s tests -v` from
  `Code/v7`. The tests use test doubles and do not validate physical serial
  hardware, the rendered UI in a browser, or end-to-end GUI/firmware operation.
