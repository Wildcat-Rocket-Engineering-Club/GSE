from flask import Flask
from flask_socketio import SocketIO
import random

app = Flask(__name__)
socketio = SocketIO(app, async_mode='gevent')

@app.route('/')
def index():
    return """
    <script src="https://cdn.socket.io/4.0.0/socket.io.min.js"></script>
    <script>
        var socket = io();
        socket.on('random', function(data){
            console.log(data);
        });
    </script>
    """

def send_random():
    while True:
        socketio.emit('random', random.random())
        socketio.sleep(0.1)

if __name__ == '__main__':
    socketio.start_background_task(send_random)
    socketio.run(app)