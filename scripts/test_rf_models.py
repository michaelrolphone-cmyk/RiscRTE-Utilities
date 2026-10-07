#!/usr/bin/env python3
"""Exercise pure RF DSP/room/event/learning models; never use physical hardware."""
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='rf-model-tests-') as output:
    for source in sorted((ROOT / 'test/native_apps').glob('rf_*_model_test.c')):
        for sanitize in (False, True):
            target = Path(output) / (source.stem + ('-san' if sanitize else ''))
            flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] if sanitize else []
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags, str(source), '-lm', '-o', str(target)], check=True, timeout=120)
            subprocess.run([str(target)], check=True, timeout=120, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
print('RF production model normal and ASan/UBSan tests passed')
