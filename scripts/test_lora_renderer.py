#!/usr/bin/env python3
"""Link real LoRa app, Portable adapter and Nova rasterizer with fake peripherals."""
import argparse, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);a=p.parse_args();system=a.system_apps.resolve()
scenes={
 'home':[], 'compose':[(3,100,100)],
 'keyboard':[(3,100,100),(20,50,210)],
 'rf':[(3,175,200)],'rf-invalid':[(3,175,200),(20,170,210)],
 'rf-keyboard':[(3,175,200),(20,100,65)],
 'history':[(3,100,150)],
 'listen-stop':[(3,50,200),(20,50,200)],
 'send-history':[(3,100,100),(20,50,210),(40,53,87),(60,200,190),(80,170,210),(100,20,20),(120,100,150),(140,100,55)],
 'nested-back':[(3,100,100),(20,50,210),(40,53,87),(60,20,20),(80,20,20),(100,100,150)],
}
for sanitized in (False,True):
 out=ROOT/'build/lora-renderer'/str(int(sanitized));out.mkdir(parents=True,exist_ok=True)
 catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
 flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RADIO_SESSION','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL']
 if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 exe=out/'lora-renderer'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(i) for i in (ROOT/'Apps',ROOT/'lib/Alarm/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include')],str(ROOT/'test/native_apps/lora_renderer_test.c'),str(ROOT/'Apps/lora_messages.c'),str(system/'lib/PortableApps/src/adapter.c'),str(catalog),'-o',str(exe)],check=True)
 for name,actions in scenes.items():
  folder=out/name;folder.mkdir(exist_ok=True);path=folder/'actions.txt';path.write_text(''.join(f'{n} {x} {y}\n' for n,x,y in actions))
  env=dict(os.environ)
  if name in ('listen-stop','send-history'):env['LORA_RENDER_PROFILE']='1'
  if name=='send-history':env['LORA_RENDER_ALLOW_SEND']='1'
  subprocess.run([str(exe),str(folder),str(path)],check=True,env=env,timeout=60)
  frames=sorted(folder.glob('frame-*.ppm'));assert frames,name
  for frame in frames:
   raw=frame.read_bytes();header=b'P6\n240 240\n255\n';assert raw.startswith(header) and len(raw)==len(header)+240*240*3
  raw=frames[-1].read_bytes()[len(header):]
  def lit(x0,y0,w,h):
   return sum(any(raw[(y*240+x)*3:(y*240+x+1)*3]) for y in range(y0,y0+h) for x in range(x0,x0+w))
  if name=='rf-invalid':assert lit(12,56,216,96)>250,'RF validation message must be visible'
  if name in ('keyboard','rf-keyboard'):assert lit(12,76,216,96)>1500,'standard keyboard cells missing'
  if name=='send-history':assert lit(12,44,216,120)>150,'transmit history details missing'
print('LoRa real renderer: ten scenes normal + ASan/UBSan, framebuffer stride guards, all leases released, manual RF only')
