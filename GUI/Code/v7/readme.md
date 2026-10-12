# GSEquel GUI V7

This folder is the V7 working copy of the V6 ground-support-equipment (GSE)
web interface. V6 remains unchanged and should be treated as the reference
baseline. V7 retains the same core operator interface and serial protocol, with
service extractions and a browser-based startup connection-selection screen.

The GUI receives telemetry and sends commands over a serial connection to the
STM32. It can also generate simulated telemetry for development.

## Current V7 baseline

- Serves the control and telemetry interface at `http://localhost:5001`.
- Reads newline-delimited JSON telemetry and writes commands over serial.
- Detects supported STM32 USB and XBee serial adapters at startup and asks the
  operator to select a detected connection.
- Offers simulated telemetry only as an explicit startup choice; simulation
  is labeled as not-live data and is not selected automatically on disconnect.
- Reconnects when a real serial connection drops.
- Logs received telemetry to timestamped CSV files in `datalogs/`.
- Retries an individual valve command once if telemetry does not confirm the
  requested state within the configured timeout.

## Setup

Install Python 3, then install the dependencies using the setup script for your
operating system:

- **Windows:** run `windows_setup_venv.bat`.
- **macOS:** open a terminal in this folder and run `bash mac_setup_venv.sh`.

Both scripts create a `venv` folder and install the packages from
`requirements.txt`. The scripts and dependency list are the setup source of
truth; edit neither to change the runtime options described below.

Activate the environment and start the app:

- **Windows:** `venv\Scripts\activate`
- **macOS:** `source venv/bin/activate`
- Both: `python app.py`

On Windows, you can alternatively double-click `run_gui.bat`. It creates the
venv if needed, activates it, installs/checks the pinned requirements, and
starts the app in a separate console, then opens the default browser to the
local setup page. Leave the app console open while using the GUI. It does not
run the administrator-only Digi driver setup.

In VS Code, you can instead run **Terminal > Run Task > Run GUI v7**. The task
uses the v7 virtual-environment interpreter directly, so activation is not
required. If the venv has not been created yet, run the setup script above
first.

The app uses port `5001` by default. Open `http://localhost:5001` in a browser
to reach the startup connection-selection screen. If a device is not listed,
check its USB connection and driver, then choose **rescan**. If the GUI cannot
open a device after selection, check the app console for serial-port errors.
An explicit port-in-use error means another GUI instance or serial-monitor
application may be holding the selected COM port; close that application and
retry. If the web server cannot start because port `5001` is occupied, close
the other application or change `PORT` in `app.py`.

## Automated tests

From this folder with the v7 virtual environment activated, run:

```text
python -m unittest discover -s tests -v
```

In VS Code, **Terminal > Run Task > Test GUI v7** runs the same tests using the
v7 virtual environment.

These unit tests cover browser startup selection, serial-device discovery and
framing, command writes, telemetry normalization, CSV output, and valve-command
retry behavior using test doubles. They do not connect to hardware or replace
approved bench validation.

## Choose a data connection

At startup, open the GUI in a browser and choose a connection on the setup
screen. The app detects supported STM32 Nucleo USB ports and the XBee adapter,
asks you to confirm a single detected connection, or lets you choose when
multiple connections are found. If no supported device is found, the page also
lists detected serial ports that do not match the known STM32 or XBee IDs.
Choose one to try receiving telemetry, select simulation, or scan again.

FT4232H ports at the same USB location are grouped as one XBee adapter; each
adapter uses the configured channel (channel A by default).

Unrecognized serial ports are opened at the configured baud rate but are
read-only: valve and launch commands are blocked because the device and
protocol are unknown. Incoming data is not verified as valid GSE telemetry.
Use a recognized STM32/XBee connection for controls.

Simulation is marked prominently as **not live hardware state**. The app never
switches to simulation automatically after a serial connection drops. An XBee
adapter being detected only confirms the local USB adapter is present; it does
not prove the remote radio link or STM32 is working.

### XBee

Plug in the XBee SX 900 adapter. The setup screen searches for its FT4232H USB
serial interface (`VID=0403`, `PID=6011`) and selects channel A by default.

### Direct STM32 USB

The STM32 firmware must use its USB serial interface: enable `USE_SERIAL_USB`
in the firmware's serial-port configuration section. Connect the Nucleo-F446RE
by USB; the GUI looks for the ST-LINK virtual COM port (`VID=0483`, `PID=374B`).

Both the GUI `BAUD_RATE` and firmware `SERIAL_BAUD` are `230400` by default.
Keep them in sync when changing the baud rate. USB serial devices may ignore
the baud-rate setting, but using the same configured value avoids surprises
when switching between transports.

## Editable options

GUI options are near the top of `app.py`. Firmware options are in the STM32
firmware source, not in the GUI or dependency setup files.

| Option | Where | Purpose |
| --- | --- | --- |
| `BAUD_RATE` | `app.py` | GUI serial speed; keep in sync with firmware `SERIAL_BAUD`. |
| `XBEE_CHANNEL_INDEX` | `app.py` | XBee FT4232H channel, where `0` selects channel A (the default). |
| `ENABLE_BOTTLE_PRESSURE`, `ENABLE_TANK_PRESSURE`, `ENABLE_CHAMBER_PRESSURE`, `ENABLE_LOADCELL` | `app.py` | Control which sensor readings the GUI processes/displays. |
| `ENABLE_GSE_FILL`, `ENABLE_GSE_DUMP`, `ENABLE_GSE_RELIEF`, `ENABLE_ROCKET_DUMP`, `ENABLE_ROCKET_OX`, `ENABLE_ROCKET_FUEL`, `ENABLE_ROCKET_RELIEF` | `app.py` | Control which valve states the GUI processes/displays. These are GUI display flags, not hardware safety interlocks. |
| `CONFIRM_TIMEOUT_S` | `app.py` | Time to wait for telemetry confirmation before a valve-command retry. |
| `MAX_RETRIES` | `app.py` | Maximum retries for an individual valve command. The default is one retry. |
| `PORT` | `app.py` | Local web-server port. The default is `5001`. |
| `USE_SERIAL_USB` | STM32 firmware source, serial-port configuration | Select USB serial instead of the firmware's hardware UART. |
| `SERIAL_BAUD` | STM32 firmware source | Firmware serial speed; keep in sync with GUI `BAUD_RATE`. |

The USB/XBee vendor and product IDs in `app.py` identify the expected devices;
they normally should not need editing. Other firmware settings, such as sensor
calibration and pin assignments, belong in the STM32 firmware source.

## Operating notes

- In XBee mode, connect the XBee module and ensure the STM32/GSE box is powered.
- The XBee RSSI lights can help indicate a radio link; they may only illuminate
  while the devices are communicating.
- For simulated telemetry, choose simulation on the startup screen and do not
  treat the display as live hardware state.
- Review the hardware state and operating procedures before sending any valve
  or launch command. This interface does not replace the team's safety checks.

## V7 development

Make new GUI changes in this folder so `GUI/Code/v6` stays available as the
unchanged reference. Keep protocol changes coordinated with the selected STM32
firmware version, and validate both simulated mode and an approved hardware
test setup before treating a change as ready.

See the [GUI changelog](../../CHANGELOG.md) for the change rationale and
validation history.

Serial device discovery, connection/reconnection, and newline-delimited serial
I/O are implemented in `serial_connection.py`. `app.py` handles incoming
telemetry and UI events, and sends command lines through that connection
module. CSV file creation, telemetry row formatting, flushing, and closing are
implemented in `csv_logger.py`; the app passes it canonical telemetry data.
Compact firmware packet expansion, valve-state extraction for the command
watchdog, and UI-enable payload preparation are implemented in
`telemetry_processing.py`. The enable switches remain configured in `app.py`.
`valve_commands.py` owns valve-command serialization and the telemetry-confirmed
retry watchdog; `app.py` keeps the retry-timeout and retry-count settings and
connects commands, telemetry updates, and serial-disconnect events to it.
Simulated telemetry generation remains in `app.py`; extracting it into its own
service can be handled as a separate readability improvement.
