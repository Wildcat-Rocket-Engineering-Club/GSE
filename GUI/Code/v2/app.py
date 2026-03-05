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
SERIAL_PORT = 'COM14'       # Change when using real STM32
# On Etan's computer:
# Left USB XBee was 'COM4'
# Right (near) USB with STM was 'COM14'
BAUD_RATE = 9600

# Toggle individual graphical elements (disable until they exist in SVG)
ENABLE_BOTTLE_PRESSURE = True
ENABLE_TANK_PRESSURE = False
ENABLE_CHAMBER_PRESSURE = False
ENABLE_LOADCELL = False
ENABLE_GSE_FILL = False
ENABLE_GSE_PYRO = False
ENABLE_GSE_RELIEF = False
ENABLE_ROCKET_PYRO = False
ENABLE_ROCKET_OX = False
ENABLE_ROCKET_FUEL = False
ENABLE_ROCKET_RELIEF = False

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
        socketio.emit('telemetry', {
            'pressure_transducers': {
                'bottle_pressure': int(random.uniform(0, 1300)),
                'tank_pressure': int(random.uniform(0, 1900)),
                'chamber_pressure': int(random.uniform(0, 700))
            },
            'gse': {
                'loadcell': round(random.uniform(0, 50), 2),
                'fill': random.choice([0, 1]),
                'pyro': random.choice([0, 1]),
                'relief': random.choice([0, 1])
            },
            'rocket': {
                'pyro': random.choice([0, 1]),
                'ox': random.choice([0, 1]),
                'fuel': random.choice([0, 1]),
                'relief': random.choice([0, 1])
            },
            'enables': {
                'bottle_pressure': ENABLE_BOTTLE_PRESSURE,
                'tank_pressure': ENABLE_TANK_PRESSURE,
                'chamber_pressure': ENABLE_CHAMBER_PRESSURE,
                'loadcell': ENABLE_LOADCELL,
                'gse_fill': ENABLE_GSE_FILL,
                'gse_pyro': ENABLE_GSE_PYRO,
                'gse_relief': ENABLE_GSE_RELIEF,
                'rocket_pyro': ENABLE_ROCKET_PYRO,
                'rocket_ox': ENABLE_ROCKET_OX,
                'rocket_fuel': ENABLE_ROCKET_FUEL,
                'rocket_relief': ENABLE_ROCKET_RELIEF
            }
        })

        time.sleep(1)


# ==============================
# SERIAL LOOP (REAL DATA)
# Expected JSON format from STM32:
# {"pressure_transducers":{"bottle_pressure":xxx,"tank_pressure":xxx,"chamber_pressure":xxx},
#  "gse":{"loadcell":0.00,"fill":0,"pyro":0,"relief":0},
#  "rocket":{"pyro":0,"ox":0,"fuel":0,"relief":0}}
# ==============================

def serial_loop():
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print(f"Serial connection opened on {SERIAL_PORT} at {BAUD_RATE} baud")
    last_time = time.time()
    msg_count = 0

    while True:
        try:
            line = ser.readline().decode().strip()
            if line:
                current_time = time.time()
                dt = current_time - last_time
                msg_count += 1
                
                print(f"\n[{msg_count}] Time since last: {dt:.3f}s | Received: {line[:80]}...")
                
                data = json.loads(line)
                
                # Add enables flags to the data
                data['enables'] = {
                    'bottle_pressure': ENABLE_BOTTLE_PRESSURE,
                    'tank_pressure': ENABLE_TANK_PRESSURE,
                    'chamber_pressure': ENABLE_CHAMBER_PRESSURE,
                    'loadcell': ENABLE_LOADCELL,
                    'gse_fill': ENABLE_GSE_FILL,
                    'gse_pyro': ENABLE_GSE_PYRO,
                    'gse_relief': ENABLE_GSE_RELIEF,
                    'rocket_pyro': ENABLE_ROCKET_PYRO,
                    'rocket_ox': ENABLE_ROCKET_OX,
                    'rocket_fuel': ENABLE_ROCKET_FUEL,
                    'rocket_relief': ENABLE_ROCKET_RELIEF
                }
                
                socketio.emit('telemetry', data)
                last_time = current_time

        except json.JSONDecodeError as e:
            print(f"JSON decode error: {e}")
            print(f"Line was: {repr(line)}")
        except UnicodeDecodeError as e:
            print(f"Decode error: {e} - may be encoding issue")
        except Exception as e:
            print(f"Serial error: {e}")


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