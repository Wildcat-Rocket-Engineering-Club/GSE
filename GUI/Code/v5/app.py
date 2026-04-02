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

# ==============================
# CONFIGURATION
# ==============================


# The COM port is different for every computer and USB port
# Go to the 'readme.md' file for instructions to find yours.
USE_FAKE_DATA = False
SERIAL_PORT   = 'COM4'
BAUD_RATE     = 230400

# CSV log location
# Get the directory where app.py is located
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# Define the 'datalogs' subfolder path
LOG_DIR = os.path.join(BASE_DIR, 'datalogs')

# Create the folder if it doesn't exist yet
if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)

# Toggle individual graphical elements
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

app       = Flask(__name__)
socketio  = SocketIO(app)
ser       = None
ser_lock  = threading.Lock()

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
        self.file   = None
        self.writer = None
        self.path   = None
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
                'timestamp':   now.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3],
                'elapsed_s':   f"{time.time() - self.start_time:.3f}",
                'bottle_psi':  pt.get('bottle_pressure',  ''),
                'tank_psi':    pt.get('tank_pressure',    ''),
                'chamber_psi': pt.get('chamber_pressure', ''),
                'loadcell_lbs': gse.get('loadcell', ''),
                'gse_fill':    gse.get('fill',    ''),
                'gse_relief':  gse.get('relief',  ''),
                'gse_dump':    gse.get('dump',     ''),
                'rkt_ox':      rkt.get('ox',      ''),
                'rkt_fuel':    rkt.get('fuel',     ''),
                'rkt_relief':  rkt.get('relief',   ''),
                'rkt_dump':    rkt.get('dump',     ''),
                'rkt_ign':     rkt.get('ign',      ''),
            }
            self.writer.writerow(row)
            self.row_count += 1
            # Flush every 20 rows — fast enough to not lose data, slow enough to not thrash disk
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
# Translates short keys from the STM32 compact format
# into the full-key format the rest of the code expects.
# Input:  {"p":{"b":100,"t":200,"c":50},"g":{"l":0,"f":0,"r":0,"d":0},"r":{"o":0,"f":0,"r":0,"d":0,"i":0}}
# Output: {"pressure_transducers":{...}, "gse":{...}, "rocket":{...}}
# ==============================

def expand_compact(d: dict) -> dict:
    """Expand short-key telemetry dict into canonical long-key form."""
    p   = d.get('p', {})
    g   = d.get('g', {})
    r   = d.get('r', {})
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
    """Return True if dict uses the short-key compact format."""
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
        time.sleep(1)


# ==============================
# SERIAL LOOP (REAL DATA)
# ==============================

def serial_loop():
    global ser

    msg_count = 0
    err_count = 0

    while True:
        try:
            print(f"Attempting serial connection on {SERIAL_PORT}...")
            s = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

            with ser_lock:
                ser = s

            print(f"Serial connected: {SERIAL_PORT} @ {BAUD_RATE}")

            last_time = time.time()

            # ===== ACTIVE READ LOOP =====
            while True:
                # Detect silent disconnects
                if not s.is_open:
                    raise serial.SerialException("Port closed")

                raw = s.readline()

                if not raw:
                    continue

                line = raw.decode('utf-8', errors='replace').strip()
                if not line or not line.startswith('{'):
                    continue

                now = time.time()
                dt = now - last_time
                last_time = now

                msg_count += 1
                print(f"[{msg_count}] dt={dt*1000:.1f}ms | {line[:120]}")

                data = json.loads(line)

                # Expand compact format if needed
                if is_compact(data):
                    data = expand_compact(data)

                # Log + emit
                csv_logger.log(data)
                socketio.emit('telemetry', add_enables(data))

        except (serial.SerialException, OSError) as e:
            print(f"Serial disconnected: {e}")

            with ser_lock:
                ser = None

            # Optional: notify frontend
            socketio.emit('serial_status', {'connected': False})

            try:
                s.close()
            except:
                pass

            time.sleep(1.5)  # small reconnect delay

        except json.JSONDecodeError as e:
            err_count += 1
            print(f"JSON err #{err_count}: {e}")

        except Exception as e:
            print(f"Unexpected serial error: {e}")
            time.sleep(1)


# ==============================
# COMMAND HANDLER
# Translates incoming browser command to compact or long-key JSON
# depending on what the STM32 expects.
# ==============================

# Map from long target name → compact key used in handleSerialCommand()
COMPACT_TARGET_MAP = {
    'gse_fill':     'gf',
    'gse_relief':   'gr',
    'gse_dump':     'gd',
    'rocket_ox':    'ro',
    'rocket_fuel':  'rf',
    'rocket_relief':'rr',
    'rocket_dump':  'rd',
    'ignite':       'ig',
}

@socketio.on('command')
def handle_command(data):
    with ser_lock:
        local_ser = ser

    if local_ser is None:
        print("Command dropped: serial not open")
        return

    try:
        # Build compact command: {"cmd":"set_valve","target":"gf","state":1}
        target_long = data.get('target', '')
        compact_target = COMPACT_TARGET_MAP.get(target_long, target_long)

        cmd = {
            'cmd':    data.get('cmd', 'set_valve'),
            'target': compact_target,
            'state':  data.get('state', 0),
        }

        cmd_str = json.dumps(cmd, separators=(',', ':')) + '\n'
        local_ser.write(cmd_str.encode())
        print(f"→ STM32: {cmd_str.strip()}")

    except Exception as e:
        print(f"Command error: {e}")


# ==============================
# MAIN
# ==============================

if __name__ == '__main__':
    csv_logger.open()

    try:
        thread = threading.Thread(
            target=fake_data_loop if USE_FAKE_DATA else serial_loop
        )
        thread.daemon = True
        thread.start()

        socketio.run(app, host='0.0.0.0', port=5000)

    finally:
        csv_logger.close()