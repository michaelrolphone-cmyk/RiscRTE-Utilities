#!/usr/bin/env python3
"""Run the storage-scaled Points model with real serialization and allocation."""
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/points-catalog'
out.mkdir(parents=True,exist_ok=True)
for native in (False,True):
    for sanitize in (False,True):
        target=out/f'catalog-{int(native)}-{int(sanitize)}'
        flags=['-DALARM_NATIVE_UTC'] if native else []
        if sanitize:
            flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                        *flags,'-I'+str(ROOT/'lib/Alarm/include'),str(ROOT/'tests/points_catalog_test.c'),
                        '-o',str(target)],check=True,timeout=120)
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
        subprocess.run([str(target)],env=env,check=True,timeout=120)
print('Points catalog raw/native UTC normal/ASan/UBSan PASS')
