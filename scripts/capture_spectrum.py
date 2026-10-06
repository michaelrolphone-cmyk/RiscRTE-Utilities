#!/usr/bin/env python3
"""Render actual Spectrum app/adapter frames using deterministic audio and touch."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--system-apps',required=True,type=Path);p.add_argument('--actions',type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
system=a.system_apps.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
catalog=out/'catalog.c';catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
flags=['-DPORTABLE_NOVA_UI','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_AUDIO_SESSION','-DPORTABLE_AUDIO_CONTINUOUS_CAPTURE','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_RETURN_APP="springboard.elf"']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
incs=[ROOT/'Apps',ROOT/'lib/Alarm/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include']
exe=out/'spectrum-renderer'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(i) for i in incs],str(ROOT/'test/native_apps/spectrum_renderer_test.c'),str(ROOT/'Apps/audio_spectrum.c'),str(system/'lib/PortableApps/src/adapter.c'),str(catalog),'-lm','-o',str(exe)],check=True)
subprocess.run([str(exe),str(out),*([str(a.actions.resolve())] if a.actions else [])],check=True,timeout=90)
