from flask import Flask, render_template
from flask_socketio import SocketIO
import threading
import time
import random
import serial
import json
import csv
import os
from datetime import datetime
import socket

# ==============================
# CONFIGURATION
# ==============================

USE_FAKE_DATA = False
SERIAL_PORT   = 'COM4'
BAUD_RATE     = 230400

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
LOG_DIR  = os.path.join(BASE_DIR, 'datalogs')
if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)

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
            print(f"⚠️  ERROR: Port {port} is already in use! Change the port and restart.")
            exit(1)

check_port(PORT)

app      = Flask(__name__)
socketio = SocketIO(app, async_mode='threading')
ser      = None
ser_lock = threading.Lock()


# ==============================
# VALVE WATCHDOG
#
# Tracks desired valve states and fires one retry if telemetry hasn't
# confirmed the change within CONFIRM_TIMEOUT_S.
#
# Safety properties:
#   - MAX_RETRIES = 1  →  at most 2 total writes per command (original + retry)
#   - Retry is suppressed if a newer command already arrived for that target
#     (desired state changed before the retry fired)
#   - Retry is suppressed if the serial port is not open
#   - Retry is suppressed if telemetry already confirmed the desired state
#   - On serial disconnect, all pending entries are cleared so stale retries
#     cannot fire after the port reconnects in an unknown hardware state
# ==============================

class ValveWatchdog:
    def __init__(self):
        # key: long target name, e.g. 'gse_fill'
        # value: {desired, sent_at, retries, cmd_str}
        self._pending = {}
        self._lock    = threading.Lock()

    def register(self, target_long: str, desired_state: int, cmd_str: str):
        """Call immediately after a successful serial write."""
        with self._lock:
            self._pending[target_long] = {
                'desired':  desired_state,
                'sent_at':  time.time(),
                'retries':  0,
                'cmd_str':  cmd_str,
            }
        print(f"[watchdog] registered '{target_long}' → {desired_state}")

    def confirm(self, telemetry_valve_states: dict):
        """
        Call once per telemetry packet with the current reported valve states.
        Keys must be long-form target names (e.g. 'gse_fill').
        Clears confirmed entries; queues a single retry for timed-out ones.
        """
        retries_to_fire = []

        with self._lock:
            to_remove = []
            for target, entry in self._pending.items():
                actual = telemetry_valve_states.get(target)

                # ── Confirmed: hardware echoes the desired state ──────────────
                if actual == entry['desired']:
                    print(f"[watchdog] confirmed '{target}' = {actual}")
                    to_remove.append(target)
                    continue

                # ── Timed out: decide whether to retry or give up ─────────────
                if time.time() - entry['sent_at'] >= CONFIRM_TIMEOUT_S:
                    if entry['retries'] < MAX_RETRIES:
                        retries_to_fire.append((target, entry['cmd_str'], entry['desired']))
                        entry['retries'] += 1
                        entry['sent_at'] = time.time()  # won't retry again after this
                    else:
                        print(
                            f"[watchdog] GAVE UP '{target}' after {entry['retries']} "
                            f"retr{'y' if entry['retries'] == 1 else 'ies'} — "
                            f"desired={entry['desired']}, last_seen={actual}"
                        )
                        to_remove.append(target)

            for t in to_remove:
                del self._pending[t]

        # Write outside the lock — serial writes can block briefly
        for target, cmd_str, desired in retries_to_fire:
            with ser_lock:
                local_ser = ser
            if local_ser is None:
                print(f"[watchdog] retry suppressed '{target}' — serial not open")
                continue
            try:
                local_ser.write(cmd_str.encode())
                print(f"[watchdog] RETRY '{target}' → {desired}  ({cmd_str.strip()})")
            except Exception as e:
                print(f"[watchdog] retry write error '{target}': {e}")

    def clear_all(self):
        """Call on serial disconnect to discard all pending retries."""
        with self._lock:
            count = len(self._pending)
            self._pending.clear()
        if count:
            print(f"[watchdog] cleared {count} pending entries on disconnect")


watchdog = ValveWatchdog()


# ==============================
# TELEMETRY → VALVE STATE EXTRACTOR
# Maps canonical telemetry fields to the long-form target names used
# by the watchdog. Must stay in sync with COMPACT_TARGET_MAP below.
# ==============================

def extract_valve_states(data: dict) -> dict:
    gse = data.get('gse', {})
    rkt = data.get('rocket', {})
    return {
        'gse_fill':      gse.get('fill',   None),
        'gse_relief':    gse.get('relief', None),
        'gse_dump':      gse.get('dump',   None),
        'rocket_ox':     rkt.get('ox',     None),
        'rocket_fuel':   rkt.get('fuel',   None),
        'rocket_relief': rkt.get('relief', None),
        'rocket_dump':   rkt.get('dump',   None),
        'ignite':        rkt.get('ign',    None),
    }


# ==============================
# CSV LOGGER
# ==============================

CSV_COLUMNS = [
    'timestamp', 'elapsed_s',
    'bottle_psi', 'tank_psi', 'chamber_psi',
    'loadcell_lbs',
    'gse_fill', 'gse_relief', 'gse_dump',
    'rkt_ox', 'rkt_fuel', 'rkt_relief', 'rkt_dump', 'rkt_ign',
]

class CSVLogger:
    def __init__(self):
        self.file       = None
        self.writer     = None
        self.path       = None
        self.start_time = None
        self.row_count  = 0

    def open(self):
        ts = datetime.now().strftime('%Y%m%d_%H%M%S')
        self.path = os.path.join(LOG_DIR, f'telemetry_{ts}.csv')
        self.file = open(self.path, 'w', newline='')
        self.writer = csv.DictWriter(self.file, fieldnames=CSV_COLUMNS)
        self.writer.writeheader()
        self.file.flush()
        self.start_time = time.time()
        print(f"CSV logger: writing to {self.path}")

    def log(self, data: dict):
        if self.writer is None:
            return
        try:
            pt  = data.get('pressure_transducers', {})
            gse = data.get('gse', {})
            rkt = data.get('rocket', {})
            now = datetime.now()
            row = {
                'timestamp':    now.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3],
                'elapsed_s':    f"{time.time() - self.start_time:.3f}",
                'bottle_psi':   pt.get('bottle_pressure',  ''),
                'tank_psi':     pt.get('tank_pressure',    ''),
                'chamber_psi':  pt.get('chamber_pressure', ''),
                'loadcell_lbs': gse.get('loadcell', ''),
                'gse_fill':     gse.get('fill',    ''),
                'gse_relief':   gse.get('relief',  ''),
                'gse_dump':     gse.get('dump',    ''),
                'rkt_ox':       rkt.get('ox',      ''),
                'rkt_fuel':     rkt.get('fuel',    ''),
                'rkt_relief':   rkt.get('relief',  ''),
                'rkt_dump':     rkt.get('dump',    ''),
                'rkt_ign':      rkt.get('ign',     ''),
            }
            self.writer.writerow(row)
            self.row_count += 1
            if self.row_count % 20 == 0:
                self.file.flush()
        except Exception as e:
            print(f"CSV log error: {e}")

    def close(self):
        if self.file:
            self.file.flush()
            self.file.close()
            print(f"CSV logger closed: {self.path} ({self.row_count} rows)")

csv_logger = CSVLogger()


# ==============================
# COMPACT JSON → CANONICAL FORM
# ==============================

def expand_compact(d: dict) -> dict:
    p = d.get('p', {})
    g = d.get('g', {})
    r = d.get('r', {})
    return {
        'pressure_transducers': {
            'bottle_pressure':  p.get('b', 0),
            'tank_pressure':    p.get('t', 0),
            'chamber_pressure': p.get('c', 0),
        },
        'gse': {
            'loadcell': g.get('l', 0),
            'fill':     g.get('f', 0),
            'relief':   g.get('r', 0),
            'dump':     g.get('d', 0),
        },
        'rocket': {
            'ox':     r.get('o', 0),
            'fuel':   r.get('f', 0),
            'relief': r.get('r', 0),
            'dump':   r.get('d', 0),
            'ign':    r.get('i', 0),
        },
    }

def is_compact(d: dict) -> bool:
    return 'p' in d or 'g' in d

def add_enables(data: dict) -> dict:
    data['enables'] = {
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
    return data


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
        socketio.emit('telemetry', add_enables(data))
        socketio.sleep(0.01)


# ==============================
# SERIAL LOOP (REAL DATA)
# ==============================

def serial_loop():
    global ser
    msg_count = 0
    err_count = 0

    while True:
        s = None
        try:
            print(f"Attempting serial connection on {SERIAL_PORT}...")
            s = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.2)
            s.reset_input_buffer()
            time.sleep(0.2)

            with ser_lock:
                ser = s

            print(f"Serial connected: {SERIAL_PORT} @ {BAUD_RATE}")
            socketio.emit('serial_status', {'connected': True})

            buffer = ""

            while True:
                if not s.is_open:
                    raise serial.SerialException("Port closed")

                try:
                    data_bytes = s.read(s.in_waiting or 1)
                except Exception as read_err:
                    raise serial.SerialException(f"Read failed: {read_err}")

                if not data_bytes:
                    continue

                chunk = data_bytes.decode('utf-8', errors='replace')
                buffer += chunk

                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()

                    if not line or not line.startswith('{'):
                        continue

                    try:
                        data = json.loads(line)
                        if is_compact(data):
                            data = expand_compact(data)

                        msg_count += 1
                        print(f"[{msg_count}] {line[:100]}")

                        # Feed every packet to the watchdog before emitting
                        watchdog.confirm(extract_valve_states(data))

                        csv_logger.log(data)
                        socketio.emit('telemetry', add_enables(data))

                    except json.JSONDecodeError as e:
                        err_count += 1
                        print(f"JSON err #{err_count}: {e} | line: {repr(line[:80])}")

        except (serial.SerialException, OSError) as e:
            print(f"Serial disconnected: {e}")

            with ser_lock:
                ser = None

            # Discard pending retries — hardware state is unknown after reconnect
            watchdog.clear_all()

            socketio.emit('serial_status', {'connected': False})

            try:
                if s and s.is_open:
                    s.close()
                    print("Serial port closed")
            except Exception as close_err:
                print(f"Error closing port: {close_err}")

            if 'PermissionError' in str(e) or 'Access is denied' in str(e):
                print("Port still releasing, waiting longer...")
                time.sleep(6)
            else:
                time.sleep(4)


# ==============================
# COMMAND HANDLER
# ==============================

COMPACT_TARGET_MAP = {
    'gse_fill':      'gf',
    'gse_relief':    'gr',
    'gse_dump':      'gd',
    'rocket_ox':     'ro',
    'rocket_fuel':   'rf',
    'rocket_relief': 'rr',
    'rocket_dump':   'rd',
    'ignite':        'ig',
}

@socketio.on('command')
def handle_command(data):
    with ser_lock:
        local_ser = ser

    if local_ser is None:
        print("Command dropped: serial not open")
        return

    try:
        target_long    = data.get('target', '')
        desired_state  = data.get('state', 0)
        compact_target = COMPACT_TARGET_MAP.get(target_long, target_long)

        cmd = {
            'cmd':    data.get('cmd', 'set_valve'),
            'target': compact_target,
            'state':  desired_state,
        }
        cmd_str = json.dumps(cmd, separators=(',', ':')) + '\n'
        local_ser.write(cmd_str.encode())
        print(f"→ STM32: {cmd_str.strip()}")

        # Register with watchdog only after the write succeeded.
        # Registering overwrites any prior pending entry for this target,
        # which correctly cancels a stale retry if the operator clicked again.
        if target_long in COMPACT_TARGET_MAP:
            watchdog.register(target_long, desired_state, cmd_str)

    except Exception as e:
        print(f"Command error: {e}")


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