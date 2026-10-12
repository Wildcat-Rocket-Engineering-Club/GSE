import unittest

from telemetry_processing import (
    add_enables,
    expand_compact,
    extract_valve_states,
    is_compact,
)


class TelemetryProcessingTests(unittest.TestCase):
    def test_expand_compact_packet_to_ui_schema(self):
        expanded = expand_compact(
            {
                "p": {"b": 101, "t": 202, "c": 303},
                "g": {"l": 4.5, "f": 1, "r": 0, "d": 1},
                "r": {"o": 1, "f": 0, "r": 1, "d": 0, "i": 1},
            }
        )

        self.assertEqual(
            expanded,
            {
                "pressure_transducers": {
                    "bottle_pressure": 101,
                    "tank_pressure": 202,
                    "chamber_pressure": 303,
                },
                "gse": {
                    "loadcell": 4.5,
                    "fill": 1,
                    "relief": 0,
                    "dump": 1,
                },
                "rocket": {
                    "ox": 1,
                    "fuel": 0,
                    "relief": 1,
                    "dump": 0,
                    "ign": 1,
                },
            },
        )

    def test_expand_compact_packet_defaults_missing_values_to_zero(self):
        expanded = expand_compact({"p": {}})

        self.assertEqual(expanded["pressure_transducers"]["bottle_pressure"], 0)
        self.assertEqual(expanded["gse"]["loadcell"], 0)
        self.assertEqual(expanded["rocket"]["ign"], 0)

    def test_is_compact_recognizes_compact_root_keys(self):
        self.assertTrue(is_compact({"p": {}}))
        self.assertTrue(is_compact({"g": {}}))
        self.assertFalse(is_compact({"r": {}}))
        self.assertFalse(is_compact({"pressure_transducers": {}}))

    def test_extract_valve_states_uses_watchdog_names(self):
        states = extract_valve_states(
            {
                "gse": {"fill": 1, "relief": 0, "dump": 1},
                "rocket": {
                    "ox": 0,
                    "fuel": 1,
                    "relief": 0,
                    "dump": 1,
                    "ign": 0,
                },
            }
        )

        self.assertEqual(states["gse_fill"], 1)
        self.assertEqual(states["rocket_fuel"], 1)
        self.assertEqual(states["ignite"], 0)
        self.assertIsNone(extract_valve_states({})["gse_fill"])

    def test_add_enables_attaches_a_copy_of_the_flags(self):
        data = {"gse": {}}
        flags = {"gse_fill": True}

        result = add_enables(data, flags)
        flags["gse_fill"] = False

        self.assertIs(result, data)
        self.assertEqual(data["enables"], {"gse_fill": True})


if __name__ == "__main__":
    unittest.main()
