from flask import Flask, redirect, render_template, request, url_for
from flask_socketio import SocketIO
import json
import random
import socket
import threading

from csv_logger import CSVLogger
from serial_connection import (
    SerialConnection,
    discover_devices,
    discover_other_serial_ports,
)
from telemetry_processing import (
    add_enables,
    expand_compact,
    extract_valve_states,
    is_compact,
)
from valve_commands import ValveCommandService

# ==============================
# CONFIGURATION
# ==============================

BAUD_RATE     = 230400

# Nucleo-F446RE boards expose the ST-LINK/V2-1 virtual COM port over USB.
STM_USB_VID = 0x0483
STM_USB_PID = 0x374B

# XBee SX 900 dev board uses an FT4232H (quad-port FTDI chip).
# VID 0x0403 is universal to all FTDI devices.
# PID 0x6011 is specific to the FT4232H — different from the FT232R (0x6001).
XBEE_VID = 0x0403
XBEE_PID = 0x6011

# The FT4232H enumerates as 4 COM ports. The XBee UART is wired to port A,
# which Windows assigns the lowest COM number of the group.
# If your board routes the XBee to a different channel, change this index (0=A, 1=B, etc.)
XBEE_CHANNEL_INDEX = 0

csv_logger = CSVLogger()
selection_lock = threading.Lock()
connection_state = {
    "mode": None,
    "details": None,
    "serial_connection": None,
    "valve_commands": None,
}

ENABLE_BOTTLE_PRESSURE  = True
ENABLE_TANK_PRESSURE    = True
ENABLE_CHAMBER_PRESSURE = True
ENABLE_LOADCELL         = True
ENABLE_GSE_FILL         = True
ENABLE_GSE_DUMP         = True
ENABLE_GSE_RELIEF       = True
ENABLE_ROCKET_DUMP      = True
ENABLE_ROCKET_OX        = True
ENABLE_ROCKET_FUEL      = True
ENABLE_ROCKET_RELIEF    = True

TELEMETRY_ENABLE_FLAGS = {
    'bottle_pressure':  ENABLE_BOTTLE_PRESSURE,
    'tank_pressure':    ENABLE_TANK_PRESSURE,
    'chamber_pressure': ENABLE_CHAMBER_PRESSURE,
    'loadcell':         ENABLE_LOADCELL,
    'gse_fill':         ENABLE_GSE_FILL,
    'gse_dump':         ENABLE_GSE_DUMP,
    'gse_relief':       ENABLE_GSE_RELIEF,
    'rocket_dump':      ENABLE_ROCKET_DUMP,
    'rocket_ox':        ENABLE_ROCKET_OX,
    'rocket_fuel':      ENABLE_ROCKET_FUEL,
    'rocket_relief':    ENABLE_ROCKET_RELIEF,
}

# ==============================
# RETRY WATCHDOG CONFIGURATION
# ==============================

# Seconds to wait for telemetry to confirm a valve state change before
# sending one retry. Set this to at least 2-3x your expected telemetry
# round-trip time so a slow packet doesn't cause a spurious resend.
CONFIRM_TIMEOUT_S = 0.5

# Hard cap on retries per command. 1 means at most 2 total writes per click:
# the original send + one retry. Never raise this without careful thought.
MAX_RETRIES = 1

# ==============================

PORT = 5001

def check_port(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        if s.connect_ex(('localhost', port)) == 0:
            raise SystemExit(
                f"ERROR: Web port {port} is already in use. Another GUI instance "
                f"or application may already be running on that port. Close it "
                f"or change PORT in app.py before restarting."
            )

app      = Flask(__name__)
socketio = SocketIO(app, async_mode='threading')


# ==============================
# FLASK ROUTE
# ==============================

@app.route('/')
def index():
    with selection_lock:
        mode = connection_state["mode"]
        details = connection_state["details"]
    if mode is None:
        devices = discover_connections()
        return render_template(
            "connection_setup.html",
            devices=devices,
            other_ports=discover_other_connections() if not devices else [],
            baud_rate=BAUD_RATE,
            error=None,
        )
    return render_template(
        'index.html', connection_mode=mode, connection_details=details
    )


@app.route('/select-connection', methods=['POST'])
def select_connection():
    selection = request.form.get("device_id")
    if selection == "refresh":
        return redirect(url_for("index"))

    if selection == "simulation":
        mode = "simulation"
        details = "SIMULATION — telemetry is generated and is not live hardware data."
        selected_port = None
    elif selection and selection.startswith("serial:"):
        other_ports = discover_other_connections()
        selected_device = next(
            (device for device in other_ports if device["id"] == selection), None
        )
        if selected_device is None:
            return render_template(
                "connection_setup.html",
                devices=[],
                other_ports=other_ports,
                baud_rate=BAUD_RATE,
                error="That serial port is no longer available. Scan again and choose a detected port.",
            ), 400
        mode = "generic_serial"
        selected_port = selected_device["port"]
        details = (
            f'Unrecognized serial device on {selected_port} '
            f'({selected_device["description"]})'
        )
    else:
        devices = discover_connections()
        selected_device = next(
            (device for device in devices if device["id"] == selection), None
        )
        if selected_device is None:
            return render_template(
                "connection_setup.html",
                devices=devices,
                other_ports=[],
                baud_rate=BAUD_RATE,
                error="That device is no longer available. Scan again and choose a detected device.",
            ), 400
        mode = selected_device["mode"]
        details = f'{selected_device["name"]} — {selected_device["details"]}'
        selected_port = selected_device["port"]

    with selection_lock:
        if connection_state["mode"] is not None:
            return redirect(url_for("index"))

        serial_connection = SerialConnection(
            use_fake_data=mode == "simulation",
            use_stm_usb=mode == "stm_usb",
            baud_rate=BAUD_RATE,
            stm_usb_vid=STM_USB_VID,
            stm_usb_pid=STM_USB_PID,
            xbee_vid=XBEE_VID,
            xbee_pid=XBEE_PID,
            xbee_channel_index=XBEE_CHANNEL_INDEX,
            selected_port=selected_port,
        )
        valve_commands = ValveCommandService(
            serial_connection,
            confirm_timeout_s=CONFIRM_TIMEOUT_S,
            max_retries=MAX_RETRIES,
        )
        connection_state.update(
            {
                "mode": mode,
                "details": details,
                "serial_connection": serial_connection,
                "valve_commands": valve_commands,
            }
        )

    socketio.start_background_task(
        fake_data_loop if mode == "simulation" else serial_loop
    )
    return redirect(url_for("index"))


def discover_connections():
    return discover_devices(
        STM_USB_VID,
        STM_USB_PID,
        XBEE_VID,
        XBEE_PID,
        XBEE_CHANNEL_INDEX,
    )


def discover_other_connections():
    return discover_other_serial_ports(
        STM_USB_VID, STM_USB_PID, XBEE_VID, XBEE_PID
    )


# ==============================
# FAKE DATA LOOP
# ==============================

def fake_data_loop():
    while True:
        data = {
            'pressure_transducers': {
                'bottle_pressure':  int(random.uniform(0, 1300)),
                'tank_pressure':    int(random.uniform(0, 1900)),
                'chamber_pressure': int(random.uniform(0, 700)),
            },
            'gse': {
                'loadcell': round(random.uniform(0, 50), 2),
                'fill':     random.choice([0, 1]),
                'dump':     random.choice([0, 1]),
                'relief':   random.choice([0, 1]),
            },
            'rocket': {
                'dump':   random.choice([0, 1]),
                'ox':     random.choice([0, 1]),
                'fuel':   random.choice([0, 1]),
                'relief': random.choice([0, 1]),
                'ign':    0,
            },
        }
        csv_logger.log(data)
        socketio.emit('telemetry', add_enables(data, TELEMETRY_ENABLE_FLAGS))
        socketio.sleep(0.01)


# ==============================
# SERIAL LOOP (REAL DATA)
# ==============================

def serial_loop():
    serial_connection = connection_state["serial_connection"]
    valve_commands = connection_state["valve_commands"]
    message_count = 0
    error_count = 0

    def process_line(line):
        nonlocal message_count, error_count
        try:
            data = json.loads(line)
            if is_compact(data):
                data = expand_compact(data)

            message_count += 1
            print(f"[{message_count}] {line[:100]}")

            # Feed every packet to the watchdog before emitting.
            valve_commands.confirm(extract_valve_states(data))

            csv_logger.log(data)
            socketio.emit('telemetry', add_enables(data, TELEMETRY_ENABLE_FLAGS))

        except json.JSONDecodeError as error:
            error_count += 1
            print(f"JSON err #{error_count}: {error} | line: {repr(line[:80])}")

    def on_connect():
        socketio.emit('serial_status', {'connected': True})

    def on_disconnect():
        # Discard pending retries because hardware state is unknown after reconnect.
        valve_commands.clear_all()
        socketio.emit('serial_status', {'connected': False})

    def on_error(message):
        socketio.emit(
            'serial_status', {'connected': False, 'error': message}
        )

    serial_connection.run(process_line, on_connect, on_disconnect, on_error)


# ==============================
# COMMAND HANDLER
# ==============================

@socketio.on('command')
def handle_command(data):
    if connection_state["mode"] == "generic_serial":
        print("Command blocked: unrecognized serial device is read-only")
        return
    commands = connection_state["valve_commands"]
    if commands is None:
        print("Command dropped: connection mode has not been selected")
        return
    commands.handle_command(data)

# ==============================
# MAIN
# ==============================

if __name__ == '__main__':
    check_port(PORT)
    csv_logger.open()
    try:
        socketio.run(app, host='0.0.0.0', port=PORT, use_reloader=False)
    finally:
        csv_logger.close()