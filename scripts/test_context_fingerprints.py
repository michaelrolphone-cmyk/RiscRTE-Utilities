#!/usr/bin/env python3
"""Temporal extractor, fusion and real Contexts extension regression suite."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--drivers', required=True, type=Path)
parser.add_argument('--runtime', required=True, type=Path)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='fingerprints-') as temporary:
    for sanitize in (False, True):
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                 '-fno-omit-frame-pointer', '-no-pie'] if sanitize else []
        for source in ('tests/context_fingerprint_test.c',
                       'test/native_apps/contexts_fingerprint_test.c'):
            target = Path(temporary) / ('check-' + str(sanitize))
            includes = (root/'Apps', root/'lib/Contexts/include',
                        args.drivers/'sdk/driver', args.runtime/'sdk/driver')
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
                            '-Wall', '-Wextra', '-Werror', *flags,
                            *['-I'+str(p) for p in includes], root/source,
                            '-lm', '-o', target], check=True)
            subprocess.run([target], check=True, timeout=120,
                           env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
