"""Stage the exact additive Runtime clock helper without changing ABI pins."""
import hashlib
import json
import shutil
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

def stage_runtime_clock(include):
    record = json.loads((ROOT / 'sdk/runtime-clock-source.json').read_text())
    source = ROOT / record['path']
    content = source.read_bytes()
    if hashlib.sha256(content).hexdigest() != record['sha256']:
        raise ValueError('Canonical Runtime clock helper hash mismatch')
    destination = include / source.name
    if destination.exists() and destination.read_bytes() != content:
        raise ValueError('Conflicting canonical SDK header: ' + source.name)
    if source.resolve() == destination.resolve():
        raise ValueError('SDK output aliases a source header')
    shutil.copyfile(source, destination)
