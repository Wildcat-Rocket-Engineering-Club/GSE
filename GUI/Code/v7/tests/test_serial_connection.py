import unittest
from types import SimpleNamespace
from unittest.mock import patch

import serial

from serial_connection import SerialConnection


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
