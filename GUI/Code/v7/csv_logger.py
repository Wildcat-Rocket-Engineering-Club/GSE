"""CSV telemetry logging for the V7 ground-station app."""

import csv
import os
import time
from datetime import datetime


CSV_COLUMNS = [
    'timestamp', 'elapsed_s',
    'bottle_psi', 'tank_psi', 'chamber_psi',
    'loadcell_lbs',
    'gse_fill', 'gse_relief', 'gse_dump',
    'rkt_ox', 'rkt_fuel', 'rkt_relief', 'rkt_dump', 'rkt_ign',
]


class CSVLogger:
    def __init__(self, log_dir=None):
        self.log_dir = log_dir or os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'datalogs'
        )
        if not os.path.exists(self.log_dir):
            os.makedirs(self.log_dir)
        self.file = None
        self.writer = None
        self.path = None
        self.start_time = None
        self.row_count = 0

    def open(self):
        ts = datetime.now().strftime('%Y%m%d_%H%M%S')
        self.path = os.path.join(self.log_dir, f'telemetry_{ts}.csv')
        self.file = open(self.path, 'w', newline='')
        self.writer = csv.DictWriter(self.file, fieldnames=CSV_COLUMNS)
        self.writer.writeheader()
        self.file.flush()
        self.start_time = time.time()
        print(f"CSV logger: writing to {self.path}")

    def log(self, data):
        if self.writer is None:
            return
        try:
            pt = data.get('pressure_transducers', {})
            gse = data.get('gse', {})
            rkt = data.get('rocket', {})
            now = datetime.now()
            row = {
                'timestamp':    now.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3],
                'elapsed_s':    f"{time.time() - self.start_time:.3f}",
                'bottle_psi':   pt.get('bottle_pressure',  ''),
                'tank_psi':     pt.get('tank_pressure',    ''),
                'chamber_psi':  pt.get('chamber_pressure', ''),
                'loadcell_lbs': gse.get('loadcell', ''),
                'gse_fill':     gse.get('fill',    ''),
                'gse_relief':   gse.get('relief',  ''),
                'gse_dump':     gse.get('dump',    ''),
                'rkt_ox':       rkt.get('ox',      ''),
                'rkt_fuel':     rkt.get('fuel',    ''),
                'rkt_relief':   rkt.get('relief',  ''),
                'rkt_dump':     rkt.get('dump',    ''),
                'rkt_ign':      rkt.get('ign',     ''),
            }
            self.writer.writerow(row)
            self.row_count += 1
            if self.row_count % 20 == 0:
                self.file.flush()
        except Exception as error:
            print(f"CSV log error: {error}")

    def close(self):
        if self.file:
            self.file.flush()
            self.file.close()
            print(f"CSV logger closed: {self.path} ({self.row_count} rows)")
