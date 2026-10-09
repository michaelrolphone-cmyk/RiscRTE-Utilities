#!/usr/bin/env python3
"""Exercise the real service with canonical PCM/IQ and failure injection."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--drivers', required=True, type=Path)
p.add_argument('--runtime', required=True, type=Path)
p.add_argument('--system-apps', type=Path)
a = p.parse_args()
with tempfile.TemporaryDirectory(prefix='contexts-service-') as tmp:
    for sanitize in (False, True):
        target = Path(tmp) / ('service-san' if sanitize else 'service')
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if sanitize else []
        includes = [ROOT / 'Apps', ROOT / 'lib/Contexts/include', a.drivers / 'sdk/driver', a.runtime / 'sdk/driver']
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                        *['-I' + str(i) for i in includes], str(ROOT / 'Services/contexts/service.c'),
                        str(ROOT / 'test/native_apps/contexts_service_test.c'), '-lm', '-o', str(target)], check=True)
        subprocess.run([str(target)], check=True, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}, timeout=120)
        temporal = Path(tmp) / ('temporal-san' if sanitize else 'temporal')
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
                        *['-I'+str(i) for i in includes],str(ROOT/'test/native_apps/contexts_temporal_test.c'),'-lm','-o',str(temporal)],check=True)
        subprocess.run([str(temporal)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=120)
        rf_only = Path(tmp) / ('rf-only-san' if sanitize else 'rf-only')
        owner_flags = ['-DCONTEXTS_TEST_OWNER', '-I' + str(a.system_apps / 'lib/PortableApps/include')] if a.system_apps else []
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                        '-DCONTEXTS_RF_ONLY=1', *['-I' + str(i) for i in includes], *owner_flags,
                        str(ROOT / 'Services/contexts/service.c'), str(ROOT / 'test/native_apps/contexts_rf_only_test.c'),
                        '-lm', '-o', str(rf_only)], check=True)
        subprocess.run([str(rf_only)], check=True, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}, timeout=120)
        temporal_rf = Path(tmp) / ('temporal-rf-san' if sanitize else 'temporal-rf')
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                        '-DCONTEXTS_RF_ONLY=1', *['-I' + str(i) for i in includes],
                        str(ROOT / 'test/native_apps/contexts_temporal_test.c'), '-lm', '-o', str(temporal_rf)], check=True)
        subprocess.run([str(temporal_rf)], check=True, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}, timeout=120)
        if a.system_apps:
            owner = Path(tmp) / ('owner-san' if sanitize else 'owner')
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                            *['-I' + str(i) for i in includes], '-I' + str(a.system_apps / 'lib/PortableApps/include'),
                            str(ROOT / 'test/native_apps/contexts_owner_export_test.c'), '-o', str(owner)], check=True)
            subprocess.run([str(owner)], check=True, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}, timeout=120)
