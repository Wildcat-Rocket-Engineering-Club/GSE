"""Serial port discovery and newline-delimited serial transport."""

import re
import threading
import time

import serial
import serial.tools.list_ports


def _port_sort_key(port):
    match = re.fullmatch(r"COM(\d+)", port.device.upper())
    if match:
        return 0, int(match.group(1))
    return 1, port.device.casefold()


def serial_open_error_message(port, error):
    error_text = str(error)
    lower_error = error_text.lower()
    if (
        isinstance(error, PermissionError)
        or "permissionerror" in lower_error
        or "access is denied" in lower_error
        or "resource busy" in lower_error
        or "device or resource busy" in lower_error
    ):
        return (
            f"ERROR: Could not open serial port {port}; it may already be in use "
            "by another GUI instance or serial-monitor application. Close the "
            f"other application and retry. Details: {error_text}"
        )
    return f"ERROR: Could not open serial port {port}. Details: {error_text}"


def discover_devices(
    stm_usb_vid, stm_usb_pid, xbee_vid, xbee_pid, xbee_channel_index
):
    """Return recognized USB serial connections for the browser setup screen."""
    ports = list(serial.tools.list_ports.comports())
    devices = []

    for port in ports:
        if port.vid == stm_usb_vid and port.pid == stm_usb_pid:
            devices.append(
                {
                    "id": f"stm32:{port.device}",
                    "mode": "stm_usb",
                    "name": "STM32 Nucleo USB",
                    "port": port.device,
                    "details": port.description,
                }
            )

    xbee_ports = sorted(
        (
            port for port in ports
            if port.vid == xbee_vid and port.pid == xbee_pid
        ),
        key=_port_sort_key,
    )
    xbee_groups = {}
    ungrouped_xbee_ports = []
    for port in xbee_ports:
        location = getattr(port, "location", None)
        serial_number = getattr(port, "serial_number", None)
        if location:
            group_id = f"location:{location.split(':', 1)[0]}"
        elif serial_number:
            group_id = f"serial:{serial_number}"
        else:
            ungrouped_xbee_ports.append(port)
            continue
        xbee_groups.setdefault(group_id, []).append(port)

    for offset in range(0, len(ungrouped_xbee_ports), 4):
        group = ungrouped_xbee_ports[offset:offset + 4]
        group_id = f"ports:{group[0].device}"
        xbee_groups[group_id] = group

    for group_id, group_ports in sorted(xbee_groups.items()):
        group_ports = sorted(group_ports, key=_port_sort_key)
        if group_ports:
            if xbee_channel_index >= len(group_ports):
                raise ValueError(
                    f"Configured XBee channel index {xbee_channel_index} is not "
                    f"available for adapter ports "
                    f"{', '.join(port.device for port in group_ports)}."
                )
            selected_port = group_ports[xbee_channel_index]
            devices.append(
                {
                    "id": f"xbee:{group_id}",
                    "mode": "xbee",
                    "name": "XBee serial adapter",
                    "port": selected_port.device,
                    "details": (
                        f"Using configured channel "
                        f"{chr(65 + xbee_channel_index)} ({selected_port.device}); "
                        f"adapter ports: {', '.join(p.device for p in group_ports)}"
                    ),
                }
            )

    return devices


def discover_other_serial_ports(stm_usb_vid, stm_usb_pid, xbee_vid, xbee_pid):
    """Return serial ports not identified as the supported STM32 or XBee."""
    ports = serial.tools.list_ports.comports()
    return [
        {
            "id": f"serial:{port.device}",
            "port": port.device,
            "description": port.description or "Unidentified serial device",
            "hardware_id": port.hwid or "Hardware ID unavailable",
        }
        for port in sorted(ports, key=_port_sort_key)
        if (port.vid, port.pid) not in {
            (stm_usb_vid, stm_usb_pid),
            (xbee_vid, xbee_pid),
        }
    ]


class SerialConnection:
    def __init__(
        self,
        use_fake_data,
        use_stm_usb,
        baud_rate,
        stm_usb_vid,
        stm_usb_pid,
        xbee_vid,
        xbee_pid,
        xbee_channel_index,
        selected_port=None,
    ):
        self.baud_rate = baud_rate
        self.port = selected_port
        self._serial = None
        self._lock = threading.Lock()

        if use_fake_data and use_stm_usb:
            raise ValueError("USE_FAKE_DATA and USE_STM_USB cannot both be enabled.")

        if use_fake_data:
            return
        if selected_port is not None:
            return
        if use_stm_usb:
            self.port = self._find_stm_usb_port(stm_usb_vid, stm_usb_pid)
        else:
            self.port = self._find_xbee_port(
                xbee_vid, xbee_pid, xbee_channel_index
            )

    @staticmethod
    def _find_xbee_port(vid, pid, channel_index):
        matches = sorted(
            [
                port for port in serial.tools.list_ports.comports()
                if port.vid == vid and port.pid == pid
            ],
            key=_port_sort_key,
        )

        if not matches:
            print("⚠️  ERROR: No XBee SX dev board found (VID=0403, PID=6011 / FT4232H).")
            print("           Check the USB cable and that FTDI drivers are installed.")
            print("           Available ports:")
            for port in serial.tools.list_ports.comports():
                print(
                    f"             {port.device}  —  {port.description}  "
                    f"(VID={hex(port.vid) if port.vid else 'n/a'}, "
                    f"PID={hex(port.pid) if port.pid else 'n/a'})"
                )
            raise SystemExit(1)

        if len(matches) % 4 != 0:
            print(
                f"⚠️  WARNING: Expected a multiple of 4 ports for FT4232H, "
                f"got {len(matches)}: {[port.device for port in matches]}"
            )

        selected = matches[channel_index]
        all_ports = [port.device for port in matches]
        print(f"FT4232H ports found: {all_ports}")
        print(
            f"Using channel {chr(65 + channel_index)} → {selected.device}  "
            f"(change XBEE_CHANNEL_INDEX if wrong)"
        )
        return selected.device

    @staticmethod
    def _find_stm_usb_port(vid, pid):
        ports = list(serial.tools.list_ports.comports())
        matches = [port for port in ports if port.vid == vid and port.pid == pid]

        if len(matches) != 1:
            if not matches:
                print(
                    "ERROR: No STM32 Nucleo-F446RE USB serial port found "
                    "(VID=0483, PID=374B / ST-LINK/V2-1)."
                )
                print("           Check the USB cable and that the ST-LINK driver is installed.")
                print("           Available ports:")
                for port in ports:
                    print(
                        f"             {port.device} - {port.description}  "
                        f"(VID={hex(port.vid) if port.vid else 'n/a'}, "
                        f"PID={hex(port.pid) if port.pid else 'n/a'})"
                    )
            else:
                print(
                    "ERROR: Multiple STM32 Nucleo-F446RE USB serial ports found: "
                    f"{[port.device for port in matches]}"
                )
                print("           Connect only the intended board.")
            raise SystemExit(1)

        selected = matches[0]
        print(f"STM32 Nucleo-F446RE USB port found: {selected.device}")
        return selected.device

    def is_open(self):
        with self._lock:
            return self._serial is not None and self._serial.is_open

    def send_line(self, line):
        with self._lock:
            if self._serial is None or not self._serial.is_open:
                return False
            self._serial.write(line.encode())
            return True

    def run(self, on_line, on_connect, on_disconnect, on_error=None):
        while True:
            current_serial = None
            try:
                print(f"Attempting serial connection on {self.port}...")
                current_serial = serial.Serial(
                    self.port, self.baud_rate, timeout=0.2
                )
                current_serial.reset_input_buffer()
                time.sleep(0.2)

                with self._lock:
                    self._serial = current_serial

                print(f"Serial connected: {self.port} @ {self.baud_rate}")
                on_connect()
                self._read_lines(current_serial, on_line)

            except (serial.SerialException, OSError) as error:
                if current_serial is None:
                    error_message = serial_open_error_message(self.port, error)
                else:
                    error_message = f"Serial disconnected on {self.port}: {error}"
                print(error_message)
                if on_error is not None:
                    on_error(error_message)

                with self._lock:
                    if self._serial is current_serial:
                        self._serial = None

                on_disconnect()

                try:
                    if current_serial and current_serial.is_open:
                        current_serial.close()
                        print("Serial port closed")
                except (serial.SerialException, OSError) as close_error:
                    print(f"Error closing port: {close_error}")

                if (
                    "PermissionError" in str(error)
                    or "Access is denied" in str(error)
                ):
                    print("Port still releasing, waiting longer...")
                    time.sleep(6)
                else:
                    time.sleep(4)

    @staticmethod
    def _read_lines(connection, on_line):
        buffer = ""
        while True:
            if not connection.is_open:
                raise serial.SerialException("Port closed")

            try:
                data_bytes = connection.read(connection.in_waiting or 1)
            except (serial.SerialException, OSError) as read_error:
                raise serial.SerialException(f"Read failed: {read_error}") from read_error

            if not data_bytes:
                continue

            buffer += data_bytes.decode("utf-8", errors="replace")
            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                line = line.strip()
                if line and line.startswith("{"):
                    on_line(line)
