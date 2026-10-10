#!/usr/bin/env python3
"""Exact X4 resident BLE profile with the one shared text-input capability."""
import argparse,json
from pathlib import Path
import x4_ble_resident_build as resident
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);resident.options(p)
 a=p.parse_args();profile=json.loads((ROOT/'Apps/x4-ble-resident-profile.json').read_text())
 if a.system_revision!=profile['system_revision'] or resident.RUNTIME!=profile['runtime_revision']:raise ValueError('X4 BLE dependency identity differs')
 side=json.loads((ROOT/'Apps/ble_scanner.json').read_text());inventory=json.loads((ROOT/'utilities-manifest.json').read_text())['ble_apps']
 if side['version']!=profile['version'] or [r['version'] for r in inventory if r['id']=='ble_scanner']!=[profile['version']]:raise ValueError('BLE version identity differs')
 c=resident.prepare(a,p,ROOT)
 record=resident.build(c,ROOT,'ble_scanner',profile['version'],profile['build_defines'],[ROOT/'Apps/ble_scanner.c'],profile['required_grants'],profile['features'])
 if record['build_defines']!=profile['build_defines'] or record['requires']!=profile['requires'] or record['required_grants']!=profile['required_grants']:raise ValueError('Installed X4 profile drift')
 if [r for r in record['requires'] if r['capability']=='ui.text-input']!=[dict(capability='ui.text-input',api=1)]:raise ValueError('Text requirement must be unique')
 record['baseline_profile']={k:profile[k] for k in ('baseline_version','baseline_source','baseline_receipt_sha256','baseline_helper_sha256')}
 record['profile_sha256']=resident.sha(ROOT/'Apps/x4-ble-resident-profile.json')
 record['catalog_identity']=dict(id='ble_scanner',version=profile['version'],file_name='ble_scanner.elf',source_manifest_sha256=resident.sha(ROOT/'Apps/ble_scanner.json'),inventory_sha256=resident.sha(ROOT/'utilities-manifest.json'))
 resident.write(c['out']/'ble_scanner/x4-native-app.json',record)
 resident.write(c['out']/'cohort-receipt.json',dict(schema=1,role='foreground',built={'ble_scanner':record},quick_render_total=0,hardware_verified=False,installable=False))
if __name__=='__main__':main()
