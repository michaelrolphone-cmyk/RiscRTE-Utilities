#!/usr/bin/env python3
"""Host cost of unchanged production catalog service plus Runtime AppDataFiles.

This is a measurement probe, not target performance qualification. Caller-side
catalog allocations and actual backend buffers are counted by requested bytes;
libc/stdio/allocator overhead and firmware/ELF residency are outside that count.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--system-apps', type=Path, required=True)
    p.add_argument('--output', type=Path, default=ROOT/'build/points-capacity/evidence.json')
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--counts', type=int, nargs='+', default=[8, 100, 1000, 2000, 2022])
    a = p.parse_args()
    a.output.parent.mkdir(parents=True, exist_ok=True)
    cflags = ['-O2', '-g', '-Wall', '-Wextra', '-Werror']
    if a.sanitize:
        cflags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie']
    includes = ['-I'+str(x) for x in [ROOT/'lib/Alarm/include', a.runtime/'sdk/app', a.runtime/'sdk/driver',
                                     a.runtime/'src', a.system_apps/'lib/PortableApps/include']]
    cc, cxx = os.environ.get('CC', 'cc'), os.environ.get('CXX', 'c++')
    run = lambda cmd: subprocess.run(list(map(str, cmd)), check=True)
    evidence = {'host_only': True, 'compiler': subprocess.check_output([cc, '--version'], text=True).splitlines()[0],
                'sanitized': a.sanitize, 'sources': {}, 'runs': []}
    for root, name in [(ROOT, 'utilities'), (a.runtime, 'runtime'), (a.system_apps, 'system')]:
        evidence['sources'][name] = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    with tempfile.TemporaryDirectory(prefix='points-capacity-build-') as temporary:
        out = Path(temporary)
        objects = []
        for source in [ROOT/'test/native_apps/points_catalog_capacity_backend.cpp', a.runtime/'src/runtime/storage/AppDataFiles.cpp']:
            obj = out/(source.stem+'.o')
            run([cxx, '-std=c++17', *cflags, *includes, '-c', source, '-o', obj])
            objects.append(obj)
        for native in (False, True):
            defs = ['-DALARM_NATIVE_UTC'] if native else ['-DPORTABLE_RTC_UTC8_DENVER']
            obj = out/'probe.o'
            run([cc, '-std=c11', *cflags, *defs, *includes, '-c', ROOT/'test/native_apps/points_catalog_capacity_probe.c', '-o', obj])
            extras = []
            if native:
                for filename in ['PortableTimeZone.c', 'PortableTimeZoneCatalog.c']:
                    target = out/(filename+'.o')
                    run([cc, '-std=c11', *cflags, *includes, '-c', a.system_apps/'lib/PortableApps/src'/filename, '-o', target])
                    extras.append(target)
            exe = out/('native' if native else 'watch')
            run([cxx, *cflags, obj, *objects, *extras, '-o', exe])
            for count in a.counts:
                for budget in ([0, 262144] if count == max(a.counts) else [0]):
                    with tempfile.TemporaryDirectory(prefix='points-capacity-storage-') as storage:
                        result = subprocess.check_output([str(exe), str(count), storage, str(budget)], text=True,
                                  env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'), timeout=180)
                        rows = [json.loads(line) for line in result.splitlines()]
                        evidence['runs'].append({'domain': 'native_utc' if native else 'watch_raw', 'count': count,
                                                 'budget': budget, 'measurements': rows})
                        cold = next(x for x in rows if x.get('operation') == 'cold')
                        print(json.dumps({'domain': evidence['runs'][-1]['domain'], 'count': count,
                                          'budget': budget, 'cold_ms': cold['elapsed_ns']/1e6,
                                          'peak': max(x.get('combined_peak', 0) for x in rows), 'error': cold['error']}), flush=True)
    evidence['source_sha256'] = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in [ROOT/'Services/alarm_service/service.c', ROOT/'Services/alarm_service/catalog.inc',
                     *[ROOT/'lib/Alarm/include'/name for name in ['PointsCatalog.h', 'PointsCatalogSchedule.h', 'PointsCatalogLedger.h']]]}
    a.output.write_text(json.dumps(evidence, indent=2)+'\n')


if __name__ == '__main__':
    main()
