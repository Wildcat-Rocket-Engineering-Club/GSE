"""Normalize firmware telemetry and prepare it for the V7 UI."""


def expand_compact(data):
    pressure = data.get('p', {})
    gse = data.get('g', {})
    rocket = data.get('r', {})
    return {
        'pressure_transducers': {
            'bottle_pressure':  pressure.get('b', 0),
            'tank_pressure':    pressure.get('t', 0),
            'chamber_pressure': pressure.get('c', 0),
        },
        'gse': {
            'loadcell': gse.get('l', 0),
            'fill':     gse.get('f', 0),
            'relief':   gse.get('r', 0),
            'dump':     gse.get('d', 0),
        },
        'rocket': {
            'ox':     rocket.get('o', 0),
            'fuel':   rocket.get('f', 0),
            'relief': rocket.get('r', 0),
            'dump':   rocket.get('d', 0),
            'ign':    rocket.get('i', 0),
        },
    }


def is_compact(data):
    return 'p' in data or 'g' in data


def extract_valve_states(data):
    gse = data.get('gse', {})
    rocket = data.get('rocket', {})
    return {
        'gse_fill':      gse.get('fill', None),
        'gse_relief':    gse.get('relief', None),
        'gse_dump':      gse.get('dump', None),
        'rocket_ox':     rocket.get('ox', None),
        'rocket_fuel':   rocket.get('fuel', None),
        'rocket_relief': rocket.get('relief', None),
        'rocket_dump':   rocket.get('dump', None),
        'ignite':        rocket.get('ign', None),
    }


def add_enables(data, enable_flags):
    data['enables'] = dict(enable_flags)
    return data
