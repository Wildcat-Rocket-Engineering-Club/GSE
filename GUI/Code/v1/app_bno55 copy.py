from flask import Flask, render_template
from flask_socketio import SocketIO
import threading
import time
import random
import serial
import json
import logging

# sequence counter for telemetry packets (useful when measuring rate)
_seq = 0
# simple debug logger setup
logging.basicConfig(level=logging.INFO, format="%(asctime)s %(message)s")

# ==============================
# CONFIGURATION
# ==============================

USE_FAKE_DATA = True       # <---- TOGGLE THIS for testing
SERIAL_PORT = 'COM13'       # Change when using real STM32
BAUD_RATE = 9600

# fake-data parameters – pick a high enough rate to exercise the UI
FAKE_HZ = 50               # desired updates per second; 0 = unrestricted
FAKE_INTERVAL = 1.0 / FAKE_HZ if FAKE_HZ > 0 else 0

# ==============================

app = Flask(__name__)
socketio = SocketIO(app)

@app.route('/')
def index():
    return render_template('index.html')


# ==============================
# FAKE DATA LOOP
# ==============================

def emit_telemetry(payload: dict):
    """Attach sequence number and timestamp, emit via socketio."""
    global _seq
    _seq += 1
    payload['seq'] = _seq
    payload['ts'] = time.time()
    socketio.emit('telemetry', payload)


def fake_data_loop():
    while True:
        pressure = random.uniform(0, 150)
        valve_state = random.choice([0, 1])

        emit_telemetry({'pressure': pressure, 'valve': valve_state})

        # yield control to the Socket.IO event loop so it can process
        # incoming connections and other events.  With eventlet/gevent
        # this is preferred to time.sleep().
        if FAKE_INTERVAL > 0:
            socketio.sleep(FAKE_INTERVAL)


# ==============================
# SERIAL LOOP (REAL DATA)
# Expected JSON format:
# {"pressure":123.4,"valve":1}
# ==============================

def serial_loop():
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE)

    while True:
        try:
            line = ser.readline().decode(errors='ignore').strip()
            parts = line.split(',')

            if len(parts) == 4:
                # Arduino prints w,x,y,z – use index 0 for w
                w = float(parts[0])

                # Scale quaternion (-1 to 1) → (0 to 600 PSI)
                pressure = (w + 1) * 300

                # Fake valve state for color test
                valve_state = 1 if pressure > 50 else 0

                emit_telemetry({'pressure': pressure, 'valve': valve_state})

        except Exception as e:
            logging.warning("Serial parse error: %s", e)

# ==============================
# MAIN
# ==============================

if __name__ == '__main__':

    if USE_FAKE_DATA:
        thread = threading.Thread(target=fake_data_loop)
    else:
        thread = threading.Thread(target=serial_loop)

    thread.daemon = True
    thread.start()

    # port override for convenience (pass number as first arg)
    import sys
    port = 5000
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            pass

    socketio.run(app, host='0.0.0.0', port=port)