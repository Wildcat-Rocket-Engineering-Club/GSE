from flask import Flask, render_template
from flask_socketio import SocketIO
import serial
import threading

SERIAL_PORT = 'COM13'
BAUD_RATE = 9600

app = Flask(__name__)
socketio = SocketIO(app, async_mode='gevent')

@app.route('/')
def index():
    return render_template('index_bno.html')


def serial_loop():
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE)

    while True:
        try:
            line = ser.readline().decode().strip()
            parts = line.split(',')

            if len(parts) == 4:
                socketio.emit('bno_data', {
                    'w': float(parts[0]),
                    'x': float(parts[1]),
                    'y': float(parts[2]),
                    'z': float(parts[3])
                })

        except Exception as e:
            print("Serial error:", e)


if __name__ == '__main__':
    socketio.start_background_task(serial_loop)
    socketio.run(app, host='0.0.0.0', port=5003)