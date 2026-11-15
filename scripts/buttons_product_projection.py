#!/usr/bin/env python3
"""Pure, exact-row BLE Buttons metadata projection. No firmware or device I/O.

Only raw rendering/input app grants move to the already-admitted scene provider.
All other original authorities remain byte-for-byte data, including unused
optional grants. Provider bond storage and unrelated application rows are never
edited. Product assembly and native admission are separate qualifications.
"""
from copy import deepcopy

RETIRED = frozenset(('display.output', 'input.touch.raw', 'input.navigation'))
APP_ID = 'ble_buttons'
MANIFEST = APP_ID + '.json'
ELF = APP_ID + '.elf'

def require(value, message):
    if not value:
        raise ValueError(message)

def declarations(manifest):
    require(manifest.get('id') == APP_ID and manifest.get('file_name') == ELF,
            'Wrong Buttons identity')
    require(manifest.get('architecture')=='xtensa-esp32s3' and manifest.get('type')=='application' and manifest.get('entry')=='app_main', 'Wrong application ABI')
    rows = manifest.get('requires', []) + manifest.get('optional', [])
    require(all(isinstance(x, dict) and isinstance(x.get('capability'), str) and
                type(x.get('api')) is int and x['api'] > 0 for x in rows),
            'Invalid capability declaration')
    require(len({x['capability'] for x in rows}) == len(rows), 'Duplicate declaration')
    return rows

def project_buttons_manifest(canonical, expected_row, original_manifest=None):
    result = deepcopy(canonical)
    rows = declarations(result)
    alarms = [x for x in rows if x['capability'] == 'alarm.service']
    grants = [x for x in expected_row.get('grants', []) if x.get('capability') == 'alarm.service']
    require(len(alarms) == len(grants) == 1 and alarms[0]['api'] == 2 and
            grants[0].get('api') in (1, 2) and grants[0].get('instance_id') == 0,
            'Missing or ambiguous original alarm ABI')
    alarms[0]['api'] = grants[0]['api']
    if original_manifest is not None:
        require(original_manifest.get('id') == APP_ID and original_manifest.get('file_name') == ELF,
                'Wrong original identity')
        for field in ('display_name', 'icon'):
            if field in original_manifest:
                result[field] = deepcopy(original_manifest[field])
            else:
                result.pop(field, None)
    return result

def project_buttons_boot(boot, expected_row, canonical, *, preferences=False):
    authority = {(x['capability'], x['api']) for x in declarations(canonical)}
    result = deepcopy(boot)
    rows = [x for x in result.get('app_capabilities', []) if x.get('manifest') == MANIFEST]
    require(len(rows) == 1 and rows[0] == expected_row, 'Selected Buttons row mismatch')
    grants = expected_row.get('grants')
    require(isinstance(grants, list) and all(isinstance(x,dict) and
            isinstance(x.get('capability'),str) and type(x.get('api')) is int and x['api'] > 0 and
            type(x.get('instance_id')) is int and x['instance_id'] >= 0 for x in grants), 'Invalid original grants')
    require({'capability':'storage.key-value','api':1,'instance_id':11} in grants,
            'Original HBv1 namespace11 missing')
    require({'capability':'bluetooth.hid','api':1,'instance_id':0} in grants,
            'Original HID authority missing')
    retained = [deepcopy(x) for x in grants if x['capability'] not in RETIRED]
    scene = {'capability':'ui.scene','api':1,'instance_id':0}
    require(scene not in retained, 'Scene grant already present')
    retained.append(scene)
    if preferences:
        # Caller must separately qualify the same namespace's native facade.
        require({'capability':'storage.key-value','api':1,'instance_id':1} in grants,
                'Original settings namespace1 missing')
        facade = {'capability':'storage.preferences','api':1,'instance_id':1}
        require(facade not in retained, 'Preferences grant already present')
        retained.append(facade)
    keys = [(x['capability'],x['api'],x['instance_id'],x.get('file')) for x in retained]
    require(len(keys) == len(set(keys)), 'Duplicate grant')
    require(all((x['capability'],x['api']) in authority for x in retained),
            'Preserved authority is undeclared; do not silently retire it')
    require(all((x['capability'],x['api']) in {(g['capability'],g['api']) for g in retained} for x in canonical.get('requires',[])), 'Required capability has no grant')
    rows[0]['grants'] = retained
    check = deepcopy(result)
    next(x for x in check['app_capabilities'] if x.get('manifest') == MANIFEST)['grants'] = deepcopy(grants)
    require(check == boot, 'Unrelated policy or provider changed')
    return result, {'retired':[x for x in grants if x['capability'] in RETIRED],
                    'added':[x for x in retained if x not in grants],
                    'preserved':[x for x in retained if x in grants],
                    'provider_bond_namespace_unchanged':True}

def update_buttons_route(boot, expected_row, original_sha256, new_sha256):
    require(all(isinstance(s,str) and len(s)==64 and all(c in '0123456789abcdef' for c in s)
                for s in (original_sha256,new_sha256)), 'Invalid ELF digest')
    result = deepcopy(boot)
    rows = [x for x in result.get('app_capabilities',[]) if x.get('manifest') == MANIFEST]
    require(len(rows)==1 and rows[0]==expected_row, 'Selected route mismatch')
    source = rows[0].get('image_source')
    if source:
        path = source.get('path','')
        require(isinstance(path,str) and path and not path.startswith('/') and
                all(p not in ('','..','.') for p in path.split('/')) and '\\' not in path and
                all(ord(c)>=32 and ord(c)!=127 for c in path) and path.endswith('/'+ELF), 'Invalid image route')
        require(type(source.get('volume_instance')) is int and source['volume_instance'] > 0,
                'Invalid source volume')
        if '/image-' in path:
            marker='image-'+original_sha256
            require([p for p in path.split('/') if p.startswith('image-')]==[marker], 'Stale content route')
            source['path']=path.replace(marker,'image-'+new_sha256)
    return result
