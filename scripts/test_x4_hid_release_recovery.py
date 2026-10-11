#!/usr/bin/env python3
"""Run the System GT911 recovery matrix using the exact packaged HID build flags."""
import argparse,json,sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__,add_help=False)
p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--matched-build',type=Path,required=True)
a,remaining=p.parse_known_args();system=a.system_apps.resolve();build=a.matched_build.resolve()
source=system/'scripts/test_x4_touch_recovery.py';body=source.read_text()
def replace(old,new):
 global body
 assert body.count(old)==1,(old,body.count(old));body=body.replace(old,new)
replace('ROOT=Path(__file__).resolve().parents[1]','ROOT=Path('+repr(str(system))+')')
replace("headers=native.stage(ROOT,runtime,out/'stage',out)",'''targets={app:json.loads((Path('''+repr(str(build))+''')/app/'x4-native-app.json').read_text()) for app in ('ble_touchpad','ble_buttons')}
for target in targets.values():
    assert target['system_source_revision']==native.git(ROOT,'rev-parse','HEAD')
    assert target['runtime_source_revision']==native.git(runtime,'rev-parse','HEAD')
    assert target['idle_policy']['helper_sha256']==digest(Path(target['idle_policy']['source']))
    assert target['source_revision']==native.git(utilities,'rev-parse','HEAD')
headers=out/'stage/include'
shutil.copytree(Path(targets['ble_touchpad']['idle_policy']['compiled_include_directory']),headers)
shutil.copytree(ROOT/'lib/PortableApps/time',out/'stage/time')''')
replace("flags=native.flags(app)+['-DHID_RENDER_PAPER','-DHID_RENDER_WATCH_TOUCH']", "flags=[f for f in targets[app]['build_defines'] if not f.startswith('-I')]+['-DHID_RENDER_PAPER','-DHID_RENDER_WATCH_TOUCH']")
replace("sources=native.sources(app,ROOT)+[path,catalog,ROOT/'test/native_apps/x4_hid_gt911_backend.c',driver]", "sources=native.sources(app,ROOT)+[ROOT/'lib/PortableApps/src/quick_radios.c',Path(targets[app]['idle_policy']['source']),path,catalog,ROOT/'test/native_apps/x4_hid_gt911_backend.c',driver]")
injection='''#include "TelemetryBroadcastV1.h"
static bool recovery_broadcast_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){(void)c;(void)p;assert(!allow);return true;}
static bool recovery_broadcast_pause(void*c){(void)c;return true;}
static bool recovery_broadcast_status(void*c,telemetry_broadcast_status_v1*p){(void)c;*p=(telemetry_broadcast_status_v1){.struct_size=sizeof(*p),.state=TELEMETRY_BROADCAST_OFF};return true;}
static int32_t recovery_broadcast_enumerate(void*c,uint32_t i,risc_telemetry_field_v1*p){(void)c;(void)i;(void)p;return 0;}
static int32_t recovery_broadcast_read(void*c,uint32_t i,int32_t*p){(void)c;(void)i;(void)p;return 0;}
static const telemetry_broadcast_v1 recovery_broadcast={1,sizeof(recovery_broadcast),NULL,recovery_broadcast_step,recovery_broadcast_pause,recovery_broadcast_status,recovery_broadcast_enumerate,recovery_broadcast_read};
'''
replace('        path.write_text(text)', '''        text=replace(text,'static bool fake_acquire(','''+repr(injection)+'''+ 'static bool fake_acquire(')
        text=replace(text,'if(!strcmp(n,"display.output")&&v==1)g->api=&display_api;', 'if(!strcmp(n,"telemetry.broadcast")&&v==1)g->api=&recovery_broadcast;else if(!strcmp(n,"display.output")&&v==1)g->api=&display_api;')
        path.write_text(text)''')
replace("(out/'evidence.json').write_text", "receipt['packaged_target_receipts']={app:dict(elf_sha256=value['elf_sha256'],version=value['version'],source_revision=value['source_revision'],system_source_revision=value['system_source_revision'],build_defines=value['build_defines']) for app,value in targets.items()}\n(out/'evidence.json').write_text")
sys.argv=[str(source),*remaining]
exec(compile(body,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
