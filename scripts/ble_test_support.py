"""One physical SDK for Scanner/adapter/shared-provider host and target builds."""
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


def stage_sdk(system, output):
    include = output / 'sdk/include'
    include.mkdir(parents=True, exist_ok=True)
    for folder in (system / 'lib/PortableApps/include', system / 'sdk/app'):
        for source in folder.glob('*.h'):
            shutil.copyfile(source, include / source.name)
    for name in ('PortableBluetoothHost.h', 'RiscBluetoothSensorsV1.h', 'RiscTelemetryV1.h'):
        source = ROOT / 'lib/Bluetooth/include' / name
        if name == 'RiscTelemetryV1.h':
            assert source.read_bytes() == (include / name).read_bytes(), 'Telemetry contract drift'
        shutil.copyfile(source, include / name)
    for name in ('PaperPresentation.h', 'PaperFrame.h'):
        (include / name).unlink(missing_ok=True)
    shutil.copytree(system / 'lib/PortableApps/time', include.parent / 'time', dirs_exist_ok=True)
    return include
