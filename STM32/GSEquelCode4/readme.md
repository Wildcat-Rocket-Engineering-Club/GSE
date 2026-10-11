# GSEquelCode4

STM32F446RE firmware for GSE data acquisition, valve control, launch sequencing,
and local SD logging. This folder is an independent firmware snapshot; keep
changes here separate from `GSEquelCode3` and the other STM32 versions.

## Build environment

- PlatformIO project using the Arduino framework; see `platformio.ini`.
- The ADS1256 driver is maintained locally in `lib/ADS1256/`. Its
  `cycleSingle3Tracked()` path is customized for the project's multiplexed
  pressure-sensor acquisition. See the repository's
  [project context](../../project-context/README.md) before changing it.
- Other dependencies are declared in `platformio.ini`.

Build from this directory with PlatformIO:

```text
pio run
```

Hardware behavior, especially ADC timing, valve operation, and launch
sequencing, must be validated on the target hardware in addition to compiling.

## Refactoring roadmap

Work through these items one at a time. Before starting each item, confirm its
scope and behavior with the user. Keep each change focused, build the firmware,
and preserve the existing serial protocol and behavior unless an intentional
change is agreed.

- [ ] **Extract ADS1256 pressure acquisition.** Move ADC setup, channel
  acquisition, and pressure conversion out of `main.cpp`; preserve pipeline
  alignment and the missing-module safeguard.
- [ ] **Extract HX711 load-cell handling.** Encapsulate initialization,
  calibration, readiness, and the existing four-sample update behavior. The
  HX711 library's multi-sample `get_units(4)` may block while it gathers
  conversions; do not describe this path as fully non-blocking without changing
  the agreed sampling behavior.
- [ ] **Extract SD logging.** Separate card/file lifecycle and row writing from
  the main loop while preserving the CSV format and error reporting.
- [ ] **Extract valve control and launch sequencing.** Group servo endpoints,
  valve state, and launch-sequence timing behind a clear interface.
- [ ] **Extract the serial protocol.** Separate command parsing and telemetry
  formatting; preserve compatibility with the ground-station GUI.
- [ ] **Simplify firmware orchestration.** Reduce `setup()` and `loop()` to
  coordinating the components and consolidate configuration/types where
  appropriate.
- [ ] **Verify the integrated refactor.** Build the firmware, check GUI
  protocol compatibility, and document required hardware validation.
- [ ] **Ground-station work (separate track).** Before changing GUI code, create
  `GUI/Code/v7` from the v6 baseline so v6 remains intact; then plan and approve
  GUI refactoring separately.

## Existing behavior and protocol

The firmware sends newline-delimited JSON telemetry over serial and accepts
serial commands from the ground station. The STM32 directly controls hardware;
GUI display-enable settings are not firmware safety interlocks. Coordinate
protocol changes with the GUI version in use.

## Fun

- Hi! It's me! Etan!