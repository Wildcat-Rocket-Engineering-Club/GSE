# GSEquel GUI V6

This folder contains the V6 ground-support-equipment (GSE) web interface. The
GUI receives telemetry and sends commands over a serial connection to the
STM32. It can also generate simulated telemetry for development.

This README describes the checked-in V6 behavior. The previous README had
V4-to-V5 notes, but a reliable V5 baseline is not available here, so those
notes are not presented as a V5-to-V6 changelog.

## Current V6 behavior

- Serves the control and telemetry interface at `http://localhost:5001`.
- Reads newline-delimited JSON telemetry and writes commands over serial.
- Selects either simulated data, the XBee adapter, or direct STM32 USB serial.
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

The app uses port `5001` by default. Open `http://localhost:5001` in a browser.
If startup reports that the serial device cannot be found, check the selected
connection mode and that the device/driver is available.

## Choose a data connection

Edit the configuration near the top of `app.py`. These two flags select the
connection mode:

| `USE_FAKE_DATA` | `USE_STM_USB` | Mode |
| --- | --- | --- |
| `True` | `False` | Simulated telemetry; no hardware connection |
| `False` | `False` | XBee serial connection (the default) |
| `False` | `True` | Direct USB serial connection to an STM32 Nucleo-F446RE |

Do not set both flags to `True`; the app rejects that configuration.

### XBee

Plug in the XBee SX 900 adapter. The app searches for its FT4232H USB serial
interface (`VID=0403`, `PID=6011`) and selects channel A by default.

### Direct STM32 USB

Set `USE_FAKE_DATA = False` and `USE_STM_USB = True` in `app.py`. The STM32
firmware must also use its USB serial interface: in the firmware source, enable
`USE_SERIAL_USB` in the serial-port configuration section. Connect the
Nucleo-F446RE by USB; the GUI looks for the ST-LINK virtual COM port
(`VID=0483`, `PID=374B`).

Both the GUI `BAUD_RATE` and firmware `SERIAL_BAUD` are `230400` by default.
Keep them in sync when changing the baud rate. USB serial devices may ignore
the baud-rate setting, but using the same configured value avoids surprises
when switching between transports.

## Editable options

GUI options are near the top of `app.py`. Firmware options are in the STM32
firmware source, not in the GUI or dependency setup files.

| Option | Where | Purpose |
| --- | --- | --- |
| `USE_FAKE_DATA` | `app.py` | Use generated telemetry instead of hardware data. |
| `USE_STM_USB` | `app.py` | Select direct STM32 USB instead of XBee when fake data is off. |
| `BAUD_RATE` | `app.py` | GUI serial speed; keep in sync with firmware `SERIAL_BAUD`. |
| `XBEE_CHANNEL_INDEX` | `app.py` | XBee FT4232H channel, where `0` selects channel A. |
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
- For simulated telemetry, use fake-data mode and do not treat the display as
  live hardware state.
- Review the hardware state and operating procedures before sending any valve
  or launch command. This interface does not replace the team's safety checks.
