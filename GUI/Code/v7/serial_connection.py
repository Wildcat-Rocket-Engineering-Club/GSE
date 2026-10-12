"""Serial port discovery and newline-delimited serial transport."""

import time
import threading

import serial
import serial.tools.list_ports


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
    ):
        self.baud_rate = baud_rate
        self.port = None
        self._serial = None
        self._lock = threading.Lock()

        if use_fake_data and use_stm_usb:
            raise ValueError("USE_FAKE_DATA and USE_STM_USB cannot both be enabled.")

        if use_fake_data:
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
            key=lambda port: port.device,
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

    def run(self, on_line, on_connect, on_disconnect):
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
                print(f"Serial disconnected: {error}")

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

                if "PermissionError" in str(error) or "Access is denied" in str(error):
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
