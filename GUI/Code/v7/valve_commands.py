"""Serialize valve commands and retry unconfirmed valve state changes."""

import json
import threading
import time


COMPACT_TARGET_MAP = {
    'gse_fill':      'gf',
    'gse_relief':    'gr',
    'gse_dump':      'gd',
    'rocket_ox':     'ro',
    'rocket_fuel':   'rf',
    'rocket_relief': 'rr',
    'rocket_dump':   'rd',
    'ignite':        'ig',
}


class ValveCommandService:
    """Route valve commands and manage bounded retries until telemetry confirms them."""

    def __init__(self, serial_connection, confirm_timeout_s, max_retries):
        self.serial_connection = serial_connection
        self.confirm_timeout_s = confirm_timeout_s
        self.max_retries = max_retries
        self._pending = {}
        self._lock = threading.Lock()

    def register(self, target_long, desired_state, command_line):
        with self._lock:
            self._pending[target_long] = {
                'desired': desired_state,
                'sent_at': time.time(),
                'retries': 0,
                'cmd_str': command_line,
            }
        print(f"[watchdog] registered '{target_long}' → {desired_state}")

    def confirm(self, telemetry_valve_states):
        retries_to_fire = []

        with self._lock:
            to_remove = []
            for target, entry in self._pending.items():
                actual = telemetry_valve_states.get(target)

                if actual == entry['desired']:
                    print(f"[watchdog] confirmed '{target}' = {actual}")
                    to_remove.append(target)
                    continue

                if time.time() - entry['sent_at'] >= self.confirm_timeout_s:
                    if entry['retries'] < self.max_retries:
                        retries_to_fire.append(
                            (target, entry['cmd_str'], entry['desired'])
                        )
                        entry['retries'] += 1
                        entry['sent_at'] = time.time()
                    else:
                        print(
                            f"[watchdog] GAVE UP '{target}' after {entry['retries']} "
                            f"retr{'y' if entry['retries'] == 1 else 'ies'} — "
                            f"desired={entry['desired']}, last_seen={actual}"
                        )
                        to_remove.append(target)

            for target in to_remove:
                del self._pending[target]

        for target, command_line, desired in retries_to_fire:
            if not self.serial_connection.is_open():
                print(f"[watchdog] retry suppressed '{target}' — serial not open")
                continue
            try:
                if not self.serial_connection.send_line(command_line):
                    print(f"[watchdog] retry suppressed '{target}' — serial not open")
                    continue
                print(
                    f"[watchdog] RETRY '{target}' → {desired}  "
                    f"({command_line.strip()})"
                )
            except Exception as error:
                print(f"[watchdog] retry write error '{target}': {error}")

    def clear_all(self):
        with self._lock:
            count = len(self._pending)
            self._pending.clear()
        if count:
            print(f"[watchdog] cleared {count} pending entries on disconnect")

    def handle_command(self, data):
        if not self.serial_connection.is_open():
            print("Command dropped: serial not open")
            return

        try:
            cmd_type = data.get('cmd', '')

            if cmd_type == 'launch':
                command_line = json.dumps(
                    {'cmd': 'launch'}, separators=(',', ':')
                ) + '\n'

                try:
                    if not self.serial_connection.send_line(command_line):
                        print("Launch send error (attempt 1): serial not open")
                        return
                    print(f"→ STM32 [1/3]: {command_line.strip()}")
                except Exception as error:
                    print(f"Launch send error (attempt 1): {error}")
                    return

                time.sleep(0.05)
                try:
                    if self.serial_connection.send_line(command_line):
                        print(f"→ STM32 [2/3]: {command_line.strip()}")
                except Exception as error:
                    print(f"Launch send error (attempt 2): {error}")

                time.sleep(0.05)
                try:
                    if self.serial_connection.send_line(command_line):
                        print(f"→ STM32 [3/3]: {command_line.strip()}")
                except Exception as error:
                    print(f"Launch send error (attempt 3): {error}")
                return

            if cmd_type in ('oa', 'ca', 'open_all', 'close_all'):
                stm32_cmd = 'oa' if cmd_type in ('oa', 'open_all') else 'ca'
                command_line = json.dumps(
                    {'cmd': stm32_cmd}, separators=(',', ':')
                ) + '\n'
                if not self.serial_connection.send_line(command_line):
                    print("Command dropped: serial not open")
                    return
                print(f"→ STM32: {command_line.strip()}")
                return

            target_long = data.get('target', '')
            desired_state = data.get('state', 0)
            compact_target = COMPACT_TARGET_MAP.get(target_long, target_long)
            command_line = json.dumps(
                {
                    'cmd': cmd_type,
                    'target': compact_target,
                    'state': desired_state,
                },
                separators=(',', ':'),
            ) + '\n'
            if not self.serial_connection.send_line(command_line):
                print("Command dropped: serial not open")
                return
            print(f"→ STM32: {command_line.strip()}")

            if target_long in COMPACT_TARGET_MAP:
                self.register(target_long, desired_state, command_line)

        except Exception as error:
            print(f"Command error: {error}")
