import unittest
from unittest.mock import MagicMock
from unittest.mock import patch

import app as gui_app


class AppConnectionSetupTests(unittest.TestCase):
    def setUp(self):
        gui_app.connection_state.update(
            {
                "mode": None,
                "details": None,
                "serial_connection": None,
                "valve_commands": None,
            }
        )
        self.client = gui_app.app.test_client()

    def tearDown(self):
        gui_app.connection_state.update(
            {
                "mode": None,
                "details": None,
                "serial_connection": None,
                "valve_commands": None,
            }
        )

    def test_setup_page_offers_simulation_when_no_devices_are_found(self):
        with (
            patch("app.discover_connections", return_value=[]),
            patch(
                "app.discover_other_connections",
                return_value=[
                    {
                        "id": "serial:COM5",
                        "port": "COM5",
                        "description": "USB serial adapter",
                        "hardware_id": "USB VID:PID=1234:5678",
                    }
                ],
            ),
        ):
            response = self.client.get("/")

        self.assertEqual(response.status_code, 200)
        self.assertIn(b"No supported device detected", response.data)
        self.assertIn(b"Other detected serial ports", response.data)
        self.assertIn(b"COM5", response.data)
        self.assertIn(b"read-only", response.data)
        self.assertIn(b"Yes, use simulated data", response.data)

    def test_multiple_devices_are_offered_for_individual_selection(self):
        devices = [
            {
                "id": "stm32:COM3",
                "mode": "stm_usb",
                "name": "STM32 Nucleo USB",
                "port": "COM3",
                "details": "Nucleo",
            },
            {
                "id": "xbee",
                "mode": "xbee",
                "name": "XBee serial adapter",
                "port": "COM4",
                "details": "Channel A",
            },
        ]
        with patch("app.discover_connections", return_value=devices):
            response = self.client.get("/")

        self.assertEqual(response.status_code, 200)
        self.assertIn(b"Multiple supported connections detected", response.data)
        self.assertIn(b"STM32 Nucleo USB", response.data)
        self.assertIn(b"XBee serial adapter", response.data)

    def test_simulation_requires_explicit_selection_and_is_labeled(self):
        with (
            patch("app.discover_connections", return_value=[]),
            patch.object(gui_app.socketio, "start_background_task") as start_task,
        ):
            response = self.client.post(
                "/select-connection", data={"device_id": "simulation"}
            )
            dashboard = self.client.get("/")

        self.assertEqual(response.status_code, 302)
        self.assertEqual(gui_app.connection_state["mode"], "simulation")
        self.assertIn(b"SIMULATED DATA", dashboard.data)
        start_task.assert_called_once()

    def test_stale_hardware_choice_is_rejected(self):
        with (
            patch("app.discover_connections", return_value=[]),
            patch.object(gui_app.socketio, "start_background_task") as start_task,
        ):
            response = self.client.post(
                "/select-connection", data={"device_id": "stm32:COM3"}
            )

        self.assertEqual(response.status_code, 400)
        self.assertIsNone(gui_app.connection_state["mode"])
        start_task.assert_not_called()

    def test_hardware_choice_uses_detected_port_without_rescanning_in_transport(self):
        device = {
            "id": "stm32:COM7",
            "mode": "stm_usb",
            "name": "STM32 Nucleo USB",
            "port": "COM7",
            "details": "ST-LINK",
        }
        with (
            patch("app.discover_connections", return_value=[device]),
            patch.object(gui_app.socketio, "start_background_task") as start_task,
        ):
            response = self.client.post(
                "/select-connection", data={"device_id": "stm32:COM7"}
            )

        self.assertEqual(response.status_code, 302)
        self.assertEqual(gui_app.connection_state["mode"], "stm_usb")
        self.assertEqual(gui_app.connection_state["serial_connection"].port, "COM7")
        start_task.assert_called_once()

    def test_unrecognized_port_is_selected_from_fresh_scan_and_read_only(self):
        ports = [
            {
                "id": "serial:COM5",
                "port": "COM5",
                "description": "USB serial adapter",
                "hardware_id": "USB VID:PID=1234:5678",
            }
        ]
        with (
            patch("app.discover_other_connections", return_value=ports),
            patch.object(gui_app.socketio, "start_background_task") as start_task,
        ):
            response = self.client.post(
                "/select-connection", data={"device_id": "serial:COM5"}
            )
            dashboard = self.client.get("/")

        self.assertEqual(response.status_code, 302)
        self.assertEqual(gui_app.connection_state["mode"], "generic_serial")
        self.assertEqual(gui_app.connection_state["serial_connection"].port, "COM5")
        self.assertIn(b"READ-ONLY", dashboard.data)
        self.assertNotIn(b"Open All Valves", dashboard.data)
        start_task.assert_called_once()

    def test_unrecognized_serial_selection_is_rejected_if_port_disappears(self):
        with (
            patch("app.discover_other_connections", return_value=[]),
            patch.object(gui_app.socketio, "start_background_task") as start_task,
        ):
            response = self.client.post(
                "/select-connection", data={"device_id": "serial:COM5"}
            )

        self.assertEqual(response.status_code, 400)
        self.assertIsNone(gui_app.connection_state["mode"])
        start_task.assert_not_called()

    def test_command_handler_blocks_commands_for_unrecognized_serial(self):
        class CommandSpy:
            called = False

            def handle_command(self, _data):
                self.called = True

        command_spy = CommandSpy()
        gui_app.connection_state.update(
            {"mode": "generic_serial", "valve_commands": command_spy}
        )

        gui_app.handle_command({"cmd": "launch"})

        self.assertFalse(command_spy.called)

    def test_check_port_reports_when_another_instance_uses_web_port(self):
        socket_context = MagicMock()
        socket_context.__enter__.return_value.connect_ex.return_value = 0
        with (
            patch("app.socket.socket", return_value=socket_context),
            self.assertRaisesRegex(SystemExit, "[Aa]nother GUI instance"),
        ):
            gui_app.check_port(5001)


if __name__ == "__main__":
    unittest.main()
