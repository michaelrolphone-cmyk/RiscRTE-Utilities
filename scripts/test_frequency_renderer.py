#!/usr/bin/env python3
"""Exercise production Frequency Generator and adapter with fake peripherals."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--legacy-source',type=Path);a=p.parse_args();s=a.system_apps.resolve()
for legacy in ([True] if a.legacy_source else [False]):
 for sanitized in (False,True):
  out=ROOT/'build/frequency-renderer'/('legacy' if legacy else 'nova')/str(int(sanitized));out.mkdir(parents=True,exist_ok=True)
  catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
  source=a.legacy_source.resolve() if legacy else ROOT/'Apps/frequency_generator.c'
  flags=['-DPORTABLE_AUDIO_SESSION','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_RETURN_APP="springboard.elf"','-DFREQUENCY_APP_SOURCE='+json.dumps(str(source))]
  if not legacy:flags+=['-DPORTABLE_NOVA_UI']
  if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  exe=out/'renderer'
  subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(d) for d in (s/'lib/PortableApps/include',s/'lib/NativeApps/include',ROOT/'lib/Alarm/include',s/'test/native_apps')],str(ROOT/'test/native_apps/frequency_renderer_test.c'),str(s/'lib/PortableApps/src/adapter.c'),*[str(s/'lib/PortableApps/src'/n) for n in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')],str(catalog),'-o',str(exe)],check=True,timeout=120)
  for scene in range(1 if legacy else 14):
   frames=out/str(scene);frames.mkdir(exist_ok=True)
   for stale in frames.glob('frame-*.ppm'):stale.unlink()
   subprocess.run([str(exe),str(frames),str(scene)],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
   images=sorted(frames.glob('frame-*.ppm'));assert images
   for image in images:
    raw=image.read_bytes();header=b'P6\n240 240\n255\n';assert raw.startswith(header) and len(raw)==len(header)+240*240*3
print('Frequency production renderer: actual app/adapter frames, controls, limits, touch/crown Back, combined input, 60-second cutoff and cleanup passed')
