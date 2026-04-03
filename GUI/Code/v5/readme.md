# Instructions

## REQUIRED BEFORE RUNNING

I MADE SCRIPTS TO SET UP THE DEPENDENCIES AND VENV.

### WINDOWS

- Make sure you have python3 installed on your computer.
  - in microsoft store, download the newest version of python & also install vscode
  - in vscode, download the 'python' extension
- Make sure you have an internet connection (downloading the dependencies)
- double-click (or run in a console) the windows_setup_venv.bat batch file.
- Let it run
- Click 'accept' and install the XBee drivers onto your computer.
- Proceed to 'checking it worked'

### MAC

- Make sure you have python3 installed on your computer.
- Make sure you have an internet connection (downloading the dependencies)
- double-click (or run in a console) the mac_setup_venv.sh shell file.
- Let it run
- Install the XBee drivers for mac (if they exist)
- Proceed to 'checking it worked'

### Checking it worked

In vscode, open the "v5" folder and open a terminal in it. You should see a green "(.venv)" text appear after a second. This is how you know that the script worked. Also in "device manager" you should see "COM & LPT PORTS" after successfully installing the XBee driver.

What changed from V4 to V5:

- Auto-reconnect to Serial if the communication drops (don't need to restart the goddamn app now)
  - also turns yellow and tells you when signal is dropped
- more robust UI
- Buttons for IGNITION SEQUENCE and ALL VALVES OPEN/CLOSE (simultaneously) for testing purposes

## Usage

### Set up the XBee

- Plug in the XBee module via USB

Now you're ready to run it!

### Actually using the script

- Make sure the Xbee is plugged in and the serial port configured (see "Set up the XBee")
- Make sure the GSE box is on
  - you can also verify signal integrity by looking at the XBee module for the "RSSI" indicator lights (3 vertically that will illuminate when they connect to each other)
  - More lights = more signal strength
  - These lights will not turn on unless the devices (STM32 and/or laptop) are actively sending data. So if they're not on, there might be an issue somewhere.
- Run "app.py" as a task from vscode
- In your web-browser of choice, open localhost:5001
- You should now be seeing the interface and can use it as you see fit
- It is now your responsibility to not kill anyone or explode the rocket
  - be smart. be safe.
