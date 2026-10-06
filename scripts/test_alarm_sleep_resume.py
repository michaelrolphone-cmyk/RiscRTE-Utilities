#!/usr/bin/env python3
"""Production alarm sleep-boundary fixtures, including exact current profile."""
import argparse, os, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--system-apps', type=Path, required=True)
p.add_argument('--runtime', type=Path, required=True)
a = p.parse_args()
out = ROOT/'build/alarm-sleep-resume'
out.mkdir(parents=True, exist_ok=True)
includes = [ROOT/'lib/Alarm/include', a.runtime/'sdk/driver', a.system_apps/'lib/PortableApps/include']
profiles = [[], ['-DPOINTS_IN_TIME_SERVICE', '-DPORTABLE_RTC_UTC8_DENVER', '-DALARM_VOLUME_CONTROL', '-DALARM_DND_CONTROL']]
for profile, defines in enumerate(profiles):
    for sanitized in (False, True):
        target = out/f'profile-{profile}-{int(sanitized)}'
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'] if sanitized else []
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                        *flags, *defines, *['-I'+str(i) for i in includes],
                        str(ROOT/'test/native_apps/alarm_sleep_resume_test.c'), '-o', str(target)], check=True, timeout=120)
        subprocess.run([str(target)], check=True, timeout=120)
print('Legacy and deployed-profile alarm sleep resume normal/ASan/UBSan passed')
