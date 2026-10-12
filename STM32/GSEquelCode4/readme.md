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

See the [STM32 changelog](../CHANGELOG.md) for the change rationale and
validation history.

- [x] **Extract ADS1256 pressure acquisition.** Move ADC setup, channel
  acquisition, and pressure conversion out of `main.cpp`; preserve pipeline
  alignment and the missing-module safeguard.
- [x] **Extract HX711 load-cell handling.** Encapsulate initialization,
  calibration, readiness, and the existing four-sample update behavior. The
  HX711 library's multi-sample `get_units(4)` may block while it gathers
  conversions; do not describe this path as fully non-blocking without changing
  the agreed sampling behavior.
- [x] **Extract SD logging.** Separate card/file lifecycle and row writing from
  the main loop. Keep the CSV header and data rows consistent; send diagnostics
  to Serial rather than inserting debug markers into the CSV.
- [x] **Extract valve control and launch sequencing.** Group servo endpoints,
  valve state, and launch-sequence timing behind a clear interface.
- [x] **Extract the serial protocol.** Move line buffering, command parsing,
  and telemetry JSON formatting into a module; keep hardware actions in
  `main.cpp` and preserve the existing wire formats.
- [x] **Simplify firmware orchestration.** Keep `setup()` and `loop()` focused
  on a small number of named initialization and update steps without adding an
  unnecessary coordinator abstraction.
- [x] **Verify the integrated refactor.** Build the firmware, check GUI
  protocol compatibility, and document required hardware validation.
- [x] **Ground-station work (separate track).** Before changing GUI code, create
  `GUI/Code/v7` from the v6 baseline so v6 remains intact; then plan and approve
  GUI refactoring separately.

## GUI v6 serial protocol

The firmware and GUI v6 agree on a 230400-baud, newline-delimited JSON serial
protocol.

- Valve commands use `{"cmd":"set_valve","target":"gf","state":1}`. The compact
  target keys are `gf`, `gr`, `gd`, `ro`, `rf`, `rr`, `rd`, and `ig`.
- Bulk commands use `{"cmd":"oa"}` (open all) and `{"cmd":"ca"}` (close all).
- Launch commands use `{"cmd":"launch"}`. GUI v6 sends this command three
  times 50 ms apart; the firmware ignores duplicates while a launch sequence
  is active and accepts a new sequence when idle or after the prior sequence
  completes.
- Telemetry uses compact `p`, `g`, and `r` objects. `p` contains bottle, tank,
  and chamber pressures (`b`, `t`, `c`); `g` contains load-cell and GSE valve
  states (`l`, `f`, `r`, `d`); `r` contains rocket valve states (`o`, `f`,
  `r`, `d`, `i`). GUI v6 expands these keys into its canonical telemetry form.

The STM32 directly controls hardware. GUI display-enable settings are not
firmware safety interlocks. Coordinate future protocol changes with the GUI
version in use.

## Hardware validation still required

Build success does not verify the attached hardware. Perform these checks only
under the team's approved test procedures, using a safe bench setup without
pressurized propellants or an active propulsion system:

- Confirm the PCA9685 starts with valves commanded closed, then verify each
  servo's calibrated open/closed endpoint and telemetry state individually.
- Verify pressure channels map to bottle/tank/chamber as expected and compare
  readings against an appropriate known reference; confirm ADS1256 readiness
  and channel alignment.
- Check the HX711 calibration with a known test load and confirm the displayed
  load-cell units and update behavior.
- Confirm the SD card creates a new numbered CSV, its header matches each data
  row, and Serial reports initialization, progress, and any write/sync errors.
- On a safe actuator test setup, measure the launch sequence at T+0, T+500 ms,
  and T+750 ms; also confirm duplicate launch frames do not restart an active
  sequence.
- Verify the GUI and STM32 serial connection at the configured baud rate and
  confirm telemetry and individual/bulk commands in both directions.

Follow approved team procedures and keep test instrumentation and actuators in
a safe state; the GUI is not a substitute for hardware interlocks or operating
procedures.

## Fun

- Hi! It's me! Etan!