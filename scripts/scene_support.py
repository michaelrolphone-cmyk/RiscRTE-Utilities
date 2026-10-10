"""Canonical SDK staging shared by the Alarms prototype's builds and tests."""
import importlib.util
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


def sdk(runtime: Path, system: Path, output: Path) -> Path:
    spec = importlib.util.spec_from_file_location('scene_sdk', system/'scripts/scene_sdk.py')
    if spec is None or spec.loader is None:
        raise ValueError('Missing System scene SDK staging script')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    include = module.stage_sdk(runtime, system, output)
    sources = list((ROOT/'lib/Alarm/include').glob('*.h'))
    sources += [system/'lib/PortableApps/include'/name for name in
                ('PortableRtcClock.h', 'PortableTime.h', 'PortableTimeZone.h',
                 'PortableTimeZonePreference.h')]
    for source in sources:
        target = include/source.name
        if target.exists() and target.read_bytes() != source.read_bytes():
            raise ValueError(f'Conflicting SDK header: {source.name}')
        shutil.copyfile(source, target)
    shutil.copytree(system/'lib/PortableApps/time', output/'time', dirs_exist_ok=True)
    return include


def time_sources(system: Path) -> list[str]:
    return [str(system/'lib/PortableApps/src'/name) for name in
            ('PortableTimeZone.c', 'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c')]
