# GSE
Ground Support Equipment Repo for WREC

## Project structure

This repository contains separate firmware and operator-HMI applications:

- `STM32/` contains versioned PlatformIO firmware snapshots. In particular,
  `GSEquelCode4` is the modular refactoring workspace; it remains separate from
  `GSEquelCode3`.
- `GUI/Code/` contains versioned ground-station applications. `v7` is the
  working copy for GUI changes, while `v6` remains the unchanged reference.
- `project-context/README.md` describes the system boundaries, versioning
  guidance, and the customized ADS1256 driver.

The STM32 directly acquires sensor data, controls valves, sequences launch
outputs, and can log to SD. The GUI presents telemetry and sends commands over
serial. Keep their JSON protocol changes coordinated; GUI display settings are
not hardware safety interlocks.

See [GUI/CHANGELOG.md](./GUI/CHANGELOG.md) and
[STM32/CHANGELOG.md](./STM32/CHANGELOG.md) for the changes made to each side,
why they were made, and the validation status to use when planning tests.
