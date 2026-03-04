# Sample Text
## Dependencies
- Python 3
- Flask
- Flask-SocketIO
- pyserial

Do:
    pip install flask flask-socketio pyserial eventlet

Configuration and testing
-------------------------
* Toggle `USE_FAKE_DATA` in `app_bno55.py` to exercise the web UI without hardware.
* When fake data is active you can adjust `FAKE_HZ` (default 50) which controls how
  frequently telemetry is generated.  A zero value means "as fast as Python will
  spin".
* The server adds a sequence number and timestamp to each packet; the HTML page
  now displays the message rate and last‑seen time to help diagnose bottlenecks.
* You can launch the script with a different port: `python app_bno55.py 5001`.

Hardware notes
--------------
The Arduino sketch sends a CSV line at ~50 Hz.  The server reads each line and
immediately emits it over Socket.IO, so the update rate is limited only by the
source.  If you see ~1 Hz on the client, the problem is most likely in the
producer (Arduino) or because the loop contains an unnecessary `sleep()`.
