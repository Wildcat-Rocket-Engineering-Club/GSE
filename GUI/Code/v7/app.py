from flask import Flask, render_template
from flask_socketio import SocketIO
import random
import json
import socket

from csv_logger import CSVLogger
from serial_connection import SerialConnection
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

# For direct USB, disable fake data and build STM32 firmware with USE_SERIAL_USB enabled.
USE_FAKE_DATA = False
USE_STM_USB   = False
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

serial_connection = SerialConnection(
    use_fake_data=USE_FAKE_DATA,
    use_stm_usb=USE_STM_USB,
    baud_rate=BAUD_RATE,
    stm_usb_vid=STM_USB_VID,
    stm_usb_pid=STM_USB_PID,
    xbee_vid=XBEE_VID,
    xbee_pid=XBEE_PID,
    xbee_channel_index=XBEE_CHANNEL_INDEX,
)

csv_logger = CSVLogger()

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

valve_commands = ValveCommandService(
    serial_connection,
    confirm_timeout_s=CONFIRM_TIMEOUT_S,
    max_retries=MAX_RETRIES,
)

PORT = 5001

def check_port(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        if s.connect_ex(('localhost', port)) == 0:
            print(f"⚠️  ERROR: Port {port} is already in use! Change the port and restart.")
            exit(1)

check_port(PORT)

app      = Flask(__name__)
socketio = SocketIO(app, async_mode='threading')


# ==============================
# FLASK ROUTE
# ==============================

@app.route('/')
def index():
    return render_template('index.html')


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

    serial_connection.run(process_line, on_connect, on_disconnect)


# ==============================
# COMMAND HANDLER
# ==============================

@socketio.on('command')
def handle_command(data):
    valve_commands.handle_command(data)

# ==============================
# MAIN
# ==============================

if __name__ == '__main__':
    csv_logger.open()
    try:
        socketio.start_background_task(fake_data_loop if USE_FAKE_DATA else serial_loop)
        socketio.run(app, host='0.0.0.0', port=PORT, use_reloader=False)
    finally:
        csv_logger.close()