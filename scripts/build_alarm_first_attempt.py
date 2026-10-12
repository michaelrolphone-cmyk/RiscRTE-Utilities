#!/usr/bin/env python3
"""Build the two opt-in first-attempt profiles and qualify target relocations."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
def run(command):
    subprocess.run([str(x) for x in command], check=True)
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--system-apps', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT/'dist/alarm-first-attempt')
    args = parser.parse_args()
    runtime, system, out = args.runtime.resolve(), args.system_apps.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get('NATIVE_APP_CC') or shutil.which('xtensa-esp32s3-elf-gcc')
    if not cc:
        raise SystemExit('Set NATIVE_APP_CC to the pinned Xtensa ESP32-S3 GCC 8.4 compiler')
    version = subprocess.check_output([cc, '--version'], text=True).splitlines()[0]
    if '8.4.0' not in version:
        raise SystemExit('GCC 8.4 required: '+version)
    mapping = out/'exports.map'
    mapping.write_text('{ global: t5_driver_get; local: *; };\n')
    common = [cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
              '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostartfiles',
              '-shared', '-Wl,--no-relax', '-Wl,--hash-style=sysv', '-Wl,--version-script='+str(mapping),
              '-Wall', '-Wextra', '-Werror']
    includes = [ROOT/'lib/Alarm/include', runtime/'sdk/driver', runtime/'sdk/app', system/'lib/PortableApps/include']
    profiles = {
        'watch-clock-change': ['POINTS_IN_TIME_SERVICE', 'PORTABLE_RTC_UTC8_DENVER', 'ALARM_VOLUME_CONTROL', 'ALARM_DND_CONTROL', 'ALARM_CLOCK_CHANGE_CONTROL'],
        'x4-startup-busy': ['POINTS_IN_TIME_SERVICE', 'ALARM_DND_CONTROL', 'ALARM_NATIVE_UTC', 'ALARM_VISUAL_ONLY', 'ALARM_SERVICE_TAGGED_V2', 'POINTS_CATALOG_SERVICE', 'ALARM_STORAGE_BUSY_RETRY']}
    rows = []
    for name, defines in profiles.items():
        elf = out/(name+'.elf')
        sources = [ROOT/'Services/alarm_service/service.c']
        if name.startswith('x4'):
            sources += [system/'lib/PortableApps/src'/n for n in ('PortableTimeZone.c', 'PortableTimeZoneCatalog.c')]
        command = [*common, *['-D'+x for x in defines], *['-I'+str(x) for x in includes], *sources, '-lgcc', '-o', elf]
        run(command)
        (out/(name+'-command.json')).write_text(json.dumps(list(map(str, command)), indent=2)+'\n')
        symbols = subprocess.check_output([cc.removesuffix('gcc')+'nm', '-D', str(elf)], text=True)
        exports = {s.split()[-1] for s in symbols.splitlines() if len(s.split())>=3 and s.split()[-2] in ('T', 'D', 'B', 'R')}
        if exports != {'t5_driver_get'}:
            raise RuntimeError('Unexpected exports: '+str(exports))
        rows.append({'name': name, 'defines': defines, 'size': elf.stat().st_size, 'sha256': digest(elf)})
    loader = out/'target-loader'
    run([os.environ.get('CC', 'cc'), '-std=gnu11', '-O1', '-g', '-I'+str(ROOT/'test/alarm-loader-stubs'),
         '-I'+str(runtime/'lib/elf_loader/include'), ROOT/'test/alarm_target_loader.c',
         runtime/'lib/elf_loader/src/esp_elf.c', runtime/'lib/elf_loader/src/arch/esp_elf_xtensa.c',
         runtime/'lib/elf_loader/src/esp_elf_validate.c', '-o', loader])
    run([loader, *[out/(n+'.elf') for n in profiles]])
    pins = {name: subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=path, text=True).strip()
            for name, path in [('utilities', ROOT), ('runtime', runtime), ('system', system)]}
    sources = {str(p.relative_to(ROOT)): digest(p) for p in [ROOT/'Services/alarm_service/service.c', ROOT/'Services/alarm_service/catalog.inc', *sorted((ROOT/'lib/Alarm/include').glob('*.h'))]}
    (out/'build-receipt.json').write_text(json.dumps({'compiler': version, 'pins': pins, 'modules': rows,
        'sources': sources, 'loader': '8 alignments, exact relocated sections and redzones', 'hardware_verified': False}, indent=2)+'\n')
if __name__ == '__main__':
    main()
