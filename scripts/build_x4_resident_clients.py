#!/usr/bin/env python3
"""Explicit resident foreground successor; legacy builders and profiles stay intact."""
import argparse
import json
from pathlib import Path
import resident_client_build as resident
from native_utility_build import flags,PROFILE
from build_native_utc_alarm_apps import NATIVE_FLAGS
ROOT=Path(__file__).resolve().parents[1]
APPS=[*PROFILE['versions'],'alarms','countdown']
def main():
    p=argparse.ArgumentParser(description=__doc__);resident.options(p)
    p.add_argument('--app',choices=APPS,action='append');a=p.parse_args()
    versions=json.loads((ROOT/'Apps/x4-resident-versions.json').read_text())
    c=resident.prepare(a,p,ROOT);alarm=json.loads((ROOT/'Apps/native-utc-alarms.json').read_text());records={}
    for name in a.app or APPS:
        profile=alarm if name in ('alarms','countdown') else PROFILE
        selected=list(NATIVE_FLAGS) if name in ('alarms','countdown') else flags(name)
        selected=[f for f in selected if f!='-DPORTABLE_QUICK_ACTIONS']
        selected+=['-DPORTABLE_BLE_BROADCAST','-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF','-DPORTABLE_PAPER_PREFERENCES','-DPORTABLE_UNPADDED_HOURS']
        if name.startswith('ble_') or name=='waterfall':selected+=['-DPORTABLE_BLE_FOREGROUND']
        if name=='waterfall':selected+=['-DPORTABLE_RADIO_CONTINUOUS_CAPTURE']
        src=[ROOT/'Apps'/(name+'.c')]
        if name=='waterfall':src+=[c['system']/'lib/NativeApps/src/SingleFloatDivisionCompat.c']
        grants=profile['common_grants']+profile['app_grants'][name]+[dict(capability='telemetry.broadcast',api=1,instance_id=0)]
        records[name]=resident.build(c,ROOT,name,versions[name],selected,src,grants,dict(native_time=True,telemetry_default='off',idle_policy='resident-host',capture_inhibits_policy=name=='waterfall',app_source='Apps/'+name+'.c'))
    resident.write(c['out']/'cohort-receipt.json',dict(schema=1,role='foreground',quick_render_total=0,built=records,hardware_verified=False,installable=False))
if __name__=='__main__':main()
