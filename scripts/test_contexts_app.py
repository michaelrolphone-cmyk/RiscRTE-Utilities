#!/usr/bin/env python3
"""Actual Contexts controller and shared NOVA raster, with bounded fake inputs."""
import argparse
import os
from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--system-apps', required=True, type=Path)
a = p.parse_args()
system = a.system_apps.resolve()
out = ROOT / 'build/contexts-app'
out.mkdir(parents=True, exist_ok=True)
for sanitize in (False, True):
    target = out / ('contexts-san' if sanitize else 'contexts')
    flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitize else []
    includes = [ROOT / 'Apps', ROOT / 'lib/Contexts/include', system / 'lib/PortableApps/include', system / 'lib/NativeApps/include']
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                    *['-I' + str(i) for i in includes], '-DCONTEXTS_NOVA_UI="' + str(system / 'lib/PortableApps/src/nova_ui.inc') + '"',
                    str(ROOT / 'test/native_apps/contexts_app_test.c'), '-o', str(target)], check=True)
    subprocess.run([str(target), str(out)], check=True, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}, timeout=90)
try:
    from PIL import Image
    for path in out.glob('*.ppm'):
        Image.open(path).save(path.with_suffix('.png'))
except ImportError:
    pass
