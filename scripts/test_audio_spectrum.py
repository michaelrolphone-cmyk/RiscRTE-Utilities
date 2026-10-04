#!/usr/bin/env python3
"""Test real Spectrum DSP and app source, never capture physical audio."""
import argparse
import os
from pathlib import Path
import subprocess
from app_manifest import validate_manifest

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--system-apps', required=True, type=Path)
a = p.parse_args()
system = a.system_apps.resolve()
validate_manifest(ROOT / 'Apps/audio_spectrum.c', 'audio_spectrum.elf')
out = ROOT / 'build/audio-spectrum-tests'
out.mkdir(parents=True, exist_ok=True)
for fixture in ('tests/spectrum_core_test.c', 'test/native_apps/audio_spectrum_test.c'):
    for sanitizer in (False, True):
        target = out / (Path(fixture).stem + ('-san' if sanitizer else ''))
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] if sanitizer else []
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                        '-I' + str(system / 'lib/PortableApps/include'), '-I' + str(system / 'lib/NativeApps/include'),
                        str(ROOT / fixture), '-lm', '-o', str(target)], check=True, timeout=120)
        subprocess.run([str(target), str(out)], check=True, timeout=90)
print('Production Audio Spectrum normal and ASan/UBSan DSP and app fixtures passed')
