import csv
import tempfile
import unittest
from pathlib import Path

from csv_logger import CSV_COLUMNS, CSVLogger


class CSVLoggerTests(unittest.TestCase):
    def test_log_writes_canonical_telemetry_columns(self):
        with tempfile.TemporaryDirectory() as log_dir:
            logger = CSVLogger(log_dir)
            logger.open()
            log_path = Path(logger.path)

            logger.log(
                {
                    "pressure_transducers": {
                        "bottle_pressure": 120,
                        "tank_pressure": 240,
                        "chamber_pressure": 360,
                    },
                    "gse": {
                        "loadcell": 12.5,
                        "fill": 1,
                        "relief": 0,
                        "dump": 1,
                    },
                    "rocket": {
                        "ox": 1,
                        "fuel": 0,
                        "relief": 1,
                        "dump": 0,
                        "ign": 0,
                    },
                }
            )
            logger.close()

            with log_path.open(newline="") as log_file:
                reader = csv.DictReader(log_file)
                rows = list(reader)

        self.assertEqual(reader.fieldnames, CSV_COLUMNS)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["bottle_psi"], "120")
        self.assertEqual(rows[0]["loadcell_lbs"], "12.5")
        self.assertEqual(rows[0]["gse_fill"], "1")
        self.assertEqual(rows[0]["rkt_ign"], "0")
        self.assertTrue(rows[0]["timestamp"])
        self.assertGreaterEqual(float(rows[0]["elapsed_s"]), 0)

    def test_log_before_open_does_not_write(self):
        with tempfile.TemporaryDirectory() as log_dir:
            logger = CSVLogger(log_dir)

            logger.log({})

            self.assertEqual(logger.row_count, 0)
            self.assertEqual(list(Path(log_dir).iterdir()), [])


if __name__ == "__main__":
    unittest.main()
