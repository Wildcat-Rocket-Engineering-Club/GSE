from flask import Flask, render_template
from flask_socketio import SocketIO
import threading
import time
import random
import serial
import json

# ==============================
# CONFIGURATION
# ==============================

USE_FAKE_DATA = False       # <---- TOGGLE THIS
SERIAL_PORT = 'COM13'       # Change when using real STM32
BAUD_RATE = 9600

# ==============================

app = Flask(__name__)
socketio = SocketIO(app)

@app.route('/')
def index():
    return render_template('index.html')


# ==============================
# FAKE DATA LOOP
# ==============================

def fake_data_loop():
    while True:
        pressure = random.uniform(0, 600)
        valve_state = random.choice([0, 1])

        socketio.emit('telemetry', {
            'pressure': pressure,
            'valve': valve_state
        })

        time.sleep(1)


# ==============================
# SERIAL LOOP (REAL DATA)
# Expected JSON format:
# {"pressure":123.4,"valve":1}
# ==============================

def serial_loop():
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE)

    while True:
        try:
            line = ser.readline().decode().strip()
            parts = line.split(',')

            if len(parts) == 4:
                w = float(parts[1])

                # Scale quaternion (-1 to 1) → (0 to 600 PSI)
                pressure = (w + 1) * 300

                # Fake valve state for color test
                valve_state = 1 if pressure > 50 else 0

                socketio.emit('telemetry', {
                    'pressure': pressure,
                    'valve': valve_state
                })

        except Exception as e:
            print("Serial parse error:", e)

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

    socketio.run(app, host='0.0.0.0', port=5000)