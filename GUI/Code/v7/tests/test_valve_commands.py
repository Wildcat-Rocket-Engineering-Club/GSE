import json
import unittest
from unittest.mock import patch

from valve_commands import ValveCommandService


class FakeSerialConnection:
    def __init__(self):
        self.open = True
        self.writes = []

    def is_open(self):
        return self.open

    def send_line(self, line):
        if not self.open:
            return False
        self.writes.append(line)
        return True


class ValveCommandServiceTests(unittest.TestCase):
    def setUp(self):
        self.serial = FakeSerialConnection()
        self.service = ValveCommandService(
            self.serial, confirm_timeout_s=0.5, max_retries=1
        )

    def test_individual_valve_command_is_mapped_and_registered(self):
        self.service.handle_command(
            {"cmd": "set_valve", "target": "gse_fill", "state": 1}
        )

        self.assertEqual(
            json.loads(self.serial.writes[0]),
            {"cmd": "set_valve", "target": "gf", "state": 1},
        )
        self.assertEqual(len(self.service._pending), 1)
        self.assertIn("gse_fill", self.service._pending)

    def test_bulk_command_is_normalized(self):
        self.service.handle_command({"cmd": "open_all"})

        self.assertEqual(self.serial.writes, ['{"cmd":"oa"}\n'])
        self.assertEqual(self.service._pending, {})

    def test_launch_command_is_sent_three_times(self):
        with patch("valve_commands.time.sleep") as sleep:
            self.service.handle_command({"cmd": "launch"})

        self.assertEqual(self.serial.writes, ['{"cmd":"launch"}\n'] * 3)
        self.assertEqual(sleep.call_count, 2)
        self.assertEqual(self.service._pending, {})

    def test_confirm_suppresses_retry_then_retries_once_and_gives_up(self):
        with patch(
            "valve_commands.time.time",
            side_effect=[10.0, 10.49, 10.5, 10.5, 11.0],
        ):
            self.service.handle_command(
                {"cmd": "set_valve", "target": "gse_fill", "state": 1}
            )
            self.service.confirm({"gse_fill": 0})
            self.assertEqual(len(self.serial.writes), 1)

            self.service.confirm({"gse_fill": 0})
            self.assertEqual(len(self.serial.writes), 2)

            self.service.confirm({"gse_fill": 0})
            self.assertEqual(len(self.serial.writes), 2)
            self.assertEqual(self.service._pending, {})

    def test_confirm_clears_matching_pending_command(self):
        self.service.handle_command(
            {"cmd": "set_valve", "target": "rocket_ox", "state": 1}
        )

        self.service.confirm({"rocket_ox": 1})

        self.assertEqual(len(self.serial.writes), 1)
        self.assertEqual(self.service._pending, {})

    def test_disconnect_clears_pending_commands(self):
        self.service.handle_command(
            {"cmd": "set_valve", "target": "gse_fill", "state": 1}
        )

        self.service.clear_all()

        self.assertEqual(self.service._pending, {})

    def test_commands_are_dropped_when_serial_is_closed(self):
        self.serial.open = False

        self.service.handle_command({"cmd": "open_all"})

        self.assertEqual(self.serial.writes, [])


if __name__ == "__main__":
    unittest.main()
