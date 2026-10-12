import unittest
from types import SimpleNamespace
from unittest.mock import patch

import serial

from serial_connection import (
    SerialConnection,
    discover_devices,
    discover_other_serial_ports,
    serial_open_error_message,
)


def fake_data_config(**overrides):
    config = {
        "use_fake_data": True,
        "use_stm_usb": False,
        "baud_rate": 230400,
        "stm_usb_vid": 0x0483,
        "stm_usb_pid": 0x374B,
        "xbee_vid": 0x0403,
        "xbee_pid": 0x6011,
        "xbee_channel_index": 0,
    }
    config.update(overrides)
    return config


class FakeSerialPort:
    is_open = True

    def __init__(self):
        self.writes = []

    def write(self, payload):
        self.writes.append(payload)
        return len(payload)


class ChunkedSerialInput:
    def __init__(self, chunks):
        self.chunks = list(chunks)

    @property
    def is_open(self):
        return bool(self.chunks)

    @property
    def in_waiting(self):
        return len(self.chunks[0]) if self.chunks else 0

    def read(self, _size):
        return self.chunks.pop(0)


class SerialConnectionTests(unittest.TestCase):
    def test_serial_open_error_identifies_port_in_use(self):
        message = serial_open_error_message(
            "COM7", serial.SerialException("PermissionError(13): Access is denied")
        )

        self.assertIn("COM7", message)
        self.assertIn("already be in use", message)
        self.assertIn("another GUI instance", message)

    def test_run_reports_port_in_use_on_initial_open_failure(self):
        class StopRetry(Exception):
            pass

        connection = SerialConnection(**fake_data_config())
        connection.port = "COM7"
        errors = []

        with (
            patch(
                "serial_connection.serial.Serial",
                side_effect=serial.SerialException(
                    "PermissionError(13): Access is denied"
                ),
            ),
            patch("serial_connection.time.sleep", side_effect=StopRetry),
        ):
            with self.assertRaises(StopRetry):
                connection.run(
                    lambda _line: None,
                    lambda: None,
                    lambda: None,
                    errors.append,
                )

        self.assertEqual(len(errors), 1)
        self.assertIn("already be in use", errors[0])

    def test_discovery_lists_each_stm32_and_one_configured_xbee_connection(self):
        ports = [
            SimpleNamespace(
                device="COM4",
                vid=0x0403,
                pid=0x6011,
                description="FT4232H Channel A",
            ),
            SimpleNamespace(
                device="COM3",
                vid=0x0403,
                pid=0x6011,
                description="FT4232H Channel B",
            ),
            SimpleNamespace(
                device="COM8",
                vid=0x0483,
                pid=0x374B,
                description="STM32 STLink",
            ),
            SimpleNamespace(
                device="COM9",
                vid=0x0483,
                pid=0x374B,
                description="STM32 STLink",
            ),
        ]
        with patch(
            "serial_connection.serial.tools.list_ports.comports",
            return_value=ports,
        ):
            devices = discover_devices(0x0483, 0x374B, 0x0403, 0x6011, 0)

        self.assertEqual(
            [device["id"] for device in devices],
            ["stm32:COM8", "stm32:COM9", "xbee:ports:COM3"],
        )
        self.assertEqual(devices[2]["port"], "COM3")
        self.assertIn("channel A", devices[2]["details"])

    def test_discovery_lists_multiple_xbee_adapters_separately(self):
        ports = [
            SimpleNamespace(
                device=f"COM{number}",
                vid=0x0403,
                pid=0x6011,
                location=f"1-{adapter}:1.{channel}",
            )
            for adapter, first_port in ((2, 3), (3, 7))
            for channel, number in enumerate(range(first_port, first_port + 4))
        ]
        with patch(
            "serial_connection.serial.tools.list_ports.comports",
            return_value=ports,
        ):
            devices = discover_devices(0x0483, 0x374B, 0x0403, 0x6011, 0)

        self.assertEqual(len(devices), 2)
        self.assertEqual(
            {device["port"] for device in devices}, {"COM3", "COM7"}
        )

    def test_xbee_channel_selection_sorts_com_ports_numerically(self):
        ports = [
            SimpleNamespace(device=f"COM{number}", vid=0x0403, pid=0x6011)
            for number in (10, 7, 9, 8)
        ]
        with patch(
            "serial_connection.serial.tools.list_ports.comports",
            return_value=ports,
        ):
            devices = discover_devices(0x0483, 0x374B, 0x0403, 0x6011, 0)

        self.assertEqual(devices[0]["port"], "COM7")

    def test_discovery_returns_empty_when_no_supported_devices_are_present(self):
        with patch(
            "serial_connection.serial.tools.list_ports.comports",
            return_value=[
                SimpleNamespace(device="COM1", vid=0x1111, pid=0x2222)
            ],
        ):
            devices = discover_devices(0x0483, 0x374B, 0x0403, 0x6011, 0)

        self.assertEqual(devices, [])

    def test_discovery_rejects_unavailable_xbee_channel(self):
        ports = [SimpleNamespace(device="COM3", vid=0x0403, pid=0x6011)]
        with patch(
            "serial_connection.serial.tools.list_ports.comports",
            return_value=ports,
        ):
            with self.assertRaisesRegex(ValueError, "channel index 1"):
                discover_devices(0x0483, 0x374B, 0x0403, 0x6011, 1)

    def test_other_serial_discovery_excludes_supported_device_ids(self):
        ports = [
            SimpleNamespace(
                device="COM3",
                vid=0x0483,
                pid=0x374B,
                description="Supported STM32",
                hwid="USB VID:PID=0483:374B",
            ),
            SimpleNamespace(
                device="COM5",
                vid=0x1234,
                pid=0x5678,
                description="Unknown adapter",
                hwid="USB VID:PID=1234:5678",
            ),
            SimpleNamespace(
                device="COM6",
                vid=None,
                pid=None,
                description="Unknown serial",
                hwid="n/a",
            ),
        ]
        with patch(
            "serial_connection.serial.tools.list_ports.comports",
            return_value=ports,
        ):
            discovered = discover_other_serial_ports(
                0x0483, 0x374B, 0x0403, 0x6011
            )

        self.assertEqual([port["port"] for port in discovered], ["COM5", "COM6"])
        self.assertEqual(discovered[0]["id"], "serial:COM5")

    def test_fake_data_mode_does_not_discover_a_port(self):
        with patch("serial_connection.serial.tools.list_ports.comports") as ports:
            connection = SerialConnection(**fake_data_config())

        ports.assert_not_called()
        self.assertIsNone(connection.port)

    def test_fake_data_and_direct_usb_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "cannot both be enabled"):
            SerialConnection(
                **fake_data_config(use_fake_data=True, use_stm_usb=True)
            )

    def test_xbee_channel_is_selected_from_sorted_matching_ports(self):
        ports = [
            SimpleNamespace(device="COM4", vid=0x0403, pid=0x6011),
            SimpleNamespace(device="COM3", vid=0x0403, pid=0x6011),
            SimpleNamespace(device="COM5", vid=0x0403, pid=0x6011),
            SimpleNamespace(device="COM6", vid=0x0403, pid=0x6011),
            SimpleNamespace(device="COM7", vid=0x1111, pid=0x2222),
        ]
        with patch(
            "serial_connection.serial.tools.list_ports.comports",
            return_value=ports,
        ):
            connection = SerialConnection(
                **fake_data_config(
                    use_fake_data=False, xbee_channel_index=1
                )
            )

        self.assertEqual(connection.port, "COM4")

    def test_send_line_writes_utf8_bytes_only_when_open(self):
        connection = SerialConnection(**fake_data_config())
        fake_port = FakeSerialPort()
        connection._serial = fake_port

        self.assertTrue(connection.send_line('{"cmd":"oa"}\n'))
        self.assertEqual(fake_port.writes, [b'{"cmd":"oa"}\n'])

        fake_port.is_open = False
        self.assertFalse(connection.send_line('{"cmd":"ca"}\n'))
        self.assertEqual(len(fake_port.writes), 1)

    def test_read_lines_dispatches_complete_json_lines(self):
        incoming = ChunkedSerialInput(
            [
                b'noise\n{"value":',
                b'1}\n\nnot-json\n{"value":2}\n',
            ]
        )
        received = []

        with self.assertRaises(serial.SerialException):
            SerialConnection._read_lines(incoming, received.append)

        self.assertEqual(received, ['{"value":1}', '{"value":2}'])


if __name__ == "__main__":
    unittest.main()
