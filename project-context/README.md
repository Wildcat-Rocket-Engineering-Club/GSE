# Project context for contributors and coding agents

This repository contains multiple parts of the ground-support system, not one
single application:

- `STM32/` contains PlatformIO/Arduino firmware experiments and versions. The
  `GSEquelCode1` through `GSEquelCode4` folders are separate snapshots; do not
  assume that a change to one is meant to update the others.
- `GUI/Code/` contains successive ground-station GUI versions. `GUI/Code/v6`
  is the reference Flask/Socket.IO interface; `GUI/Code/v7` is its separate
  working copy. Both communicate with the STM32 over newline-delimited JSON on
  a serial connection and support simulated telemetry.
- Hardware drawings, test data, and other supporting material live in their
  own top-level folders.

The STM32 is responsible for direct hardware work such as sensor acquisition,
valve actuation, launch sequencing, and local SD logging. The ground station is
the operator HMI: it displays telemetry and sends commands through either an
XBee serial link or direct USB serial. The GUI's display-enable settings are
not firmware safety interlocks. Keep command/telemetry changes coordinated
between both applications.

## Customized ADS1256 driver

The firmware folders carry a local ADS1256 library under
`STM32/<firmware>/lib/ADS1256/`; it is not just an untouched upstream copy. In
particular, `ADS1256::cycleSingle3Tracked()` is a project-specific multi-channel
acquisition path. It changes the MUX, discards the first conversion after that
change, reads the following conversion, and reports the channel associated
with the result to account for the acquisition pipeline.

This custom path appears in several checked-in firmware snapshots, so preserve
it when copying or updating the library. Its acquisition behavior is
specialized for this project; no measured speedup or benchmark is recorded
here. Verify the ADC timing and channel alignment on hardware after changing
it. The library's DRDY waits are blocking and unbounded, so a startup
presence-check does not protect against the ADC being disconnected later.

## Working safely across versions

- First identify the intended firmware snapshot and GUI version; avoid editing
  similarly named copies by assumption.
- Firmware build settings and library dependencies are defined by the relevant
  folder's `platformio.ini`. The GUI's Python dependencies and setup are
  defined under its version folder.
- Keep the serial JSON protocol compatible across firmware and GUI, and
  validate command and telemetry changes on both sides.
- Build and test the selected application where its toolchain is available.
  Hardware behavior, especially ADC timing and valve control, still requires
  hardware validation.
- `GUI/CHANGELOG.md` and `STM32/CHANGELOG.md` record the changes and their
  rationale separately for each application. Use them alongside the selected
  version's README when planning bench or integration tests.
