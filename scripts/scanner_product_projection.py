#!/usr/bin/env python3
"""Pure Scanner-only authority/route projection; no device/backend app logic.

Callers supply decoded published boot and its exact selected Scanner row. No
provider, activation, unrelated application, settings, or file bytes are edited
here. Full assembly/admission belongs to the owning product qualification.
"""
from copy import deepcopy
import hashlib
import json

RETIRED = frozenset(('display.output', 'input.touch.raw', 'input.navigation',
                    'board.battery', 'net.wifi', 'motion.accel',
                    'input.power-key', 'power.sleep', 'display.power'))
PRESERVED = frozenset(('alarm.service', 'bluetooth.sensors', 'bluetooth.hci', 'storage.key-value',
                     'storage.key-value.atomic', 'storage.preferences',
                     'storage.shared-data', 'contexts.service',
                     'telemetry.broadcast', 'ui.text-input', 'rtc.clock',
                     'runtime.realtime'))

def require(value, message):
    if not value:
        raise ValueError(message)

def project_scanner_manifest(canonical, expected_row):
    """Select the original granted alarm ABI in metadata; executable is shared."""
    result = deepcopy(canonical)
    declarations = result.get('requires', []) + result.get('optional', [])
    require(len({x['capability'] for x in declarations}) == len(declarations),
            'Duplicate capability declaration')
    alarms = [x for x in declarations if x['capability'] == 'alarm.service']
    grants = [x for x in expected_row['grants'] if x['capability'] == 'alarm.service']
    require(len(alarms) == len(grants) == 1 and grants[0]['api'] in (1, 2) and
            grants[0]['instance_id'] == 0, 'Missing or ambiguous original alarm authority')
    require(alarms[0]['api'] == 2, 'Unexpected canonical alarm ABI')
    alarms[0]['api'] = grants[0]['api']
    return result

def project_scanner_boot(boot, expected_row, canonical, *, preferences=False):
    """Preserve all original data authorities; optionally add same-ns1 facade.

    `preferences=True` is a deliberate migration requiring separate proof of
    native storage.preferences availability, not inferred from KV permission.
    Watch native0052 is qualified for that facade; existing atomic lanes need
    no additional preference authority.
    """
    require(canonical.get('id') == 'ble_scanner' and canonical.get('file_name') == 'ble_scanner.elf', 'Wrong app identity')
    declared = canonical.get('requires', []) + canonical.get('optional', [])
    require(all(isinstance(x.get('api'), int) and x['api'] > 0 for x in declared), 'Noncanonical API')
    authority = {(x['capability'], x['api']) for x in declared}
    require(len(authority) == len(declared), 'Duplicate declaration')
    result = deepcopy(boot)
    rows = [x for x in result.get('app_capabilities', []) if x.get('manifest') == 'ble_scanner.json']
    require(len(rows) == 1 and rows[0] == expected_row, 'Selected Scanner row mismatch')
    row = rows[0]
    grants = row.get('grants')
    require(isinstance(grants, list), 'Missing grants')
    require(all(x.get('capability') in RETIRED | PRESERVED for x in grants), 'Unreviewed original authority')
    retained = [deepcopy(x) for x in grants if x['capability'] not in RETIRED]
    require(any(x == {'capability':'storage.key-value', 'api':1, 'instance_id':1} for x in retained), 'Original alias namespace1 missing')
    require(any(x == {'capability':'bluetooth.sensors', 'api':1, 'instance_id':0} for x in retained), 'Original sensor authority missing')
    require(any(x == {'capability':'bluetooth.hci', 'api':1, 'instance_id':16} for x in retained), 'Original radio authority missing')
    retained.append({'capability':'ui.scene', 'api':1, 'instance_id':0})
    if preferences:
        facade = {'capability':'storage.preferences', 'api':1, 'instance_id':1}
        require(facade not in retained, 'Facade already present')
        retained.append(facade)
    keys = [(x['capability'], x['api'], x['instance_id'], x.get('file')) for x in retained]
    require(len(keys) == len(set(keys)), 'Duplicate grant')
    require(all((x['capability'], x['api']) in authority for x in retained), 'Undeclared preserved authority')
    row['grants'] = retained
    # Only the Scanner grants can differ; distribution metadata remains exact.
    check = deepcopy(result)
    next(x for x in check['app_capabilities'] if x.get('manifest') == 'ble_scanner.json')['grants'] = deepcopy(expected_row['grants'])
    require(check == boot, 'Unrelated policy changed')
    return result, {'retired': [x for x in expected_row['grants'] if x['capability'] in RETIRED],
                    'added': [x for x in retained if x not in expected_row['grants']],
                    'preserved': [x for x in retained if x in expected_row['grants']],
                    'scope':'Only Scanner grants; host/scene duty equivalence and final admission qualified separately.'}

def update_scanner_route(boot, expected_row, original_sha256, new_sha256):
    """Only content-addressed app routes change; simple SD/internal routes stay."""
    require(all(len(s) == 64 and all(c in '0123456789abcdef' for c in s) for s in (original_sha256, new_sha256)), 'Invalid SHA256')
    result = deepcopy(boot)
    rows = [x for x in result['app_capabilities'] if x.get('manifest') == 'ble_scanner.json']
    require(len(rows) == 1 and rows[0] == expected_row, 'Selected route mismatch')
    source = rows[0].get('image_source')
    if source:
        path = source.get('path', '')
        require(path and not path.startswith('/') and '..' not in path.split('/') and path.endswith('/ble_scanner.elf'), 'Invalid app path')
        marker = 'image-' + original_sha256
        if '/image-' in path:
            require(path.count(marker) == 1, 'Unexpected content-addressed route')
            source['path'] = path.replace(marker, 'image-' + new_sha256)
    return result
