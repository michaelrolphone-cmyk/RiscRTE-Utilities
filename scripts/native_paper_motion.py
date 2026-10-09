"""Explicit native paper sheet selection; existing native/Watch pins stay fixed."""
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROFILE = json.loads((ROOT/'Apps/native-paper-motion.json').read_text())
DEFINE = '-DPORTABLE_PAPER_TRANSITIONS'


def options(parser):
    parser.add_argument('--paper-transitions', action='store_true',
                        help='Opt-in native paper Quick Controls pull-down motion')
    parser.add_argument('--motion-system', type=Path,
                        help='Clean exact System motion adapter checkout')


def select(args, parser, fallback):
    if bool(args.paper_transitions) != bool(args.motion_system):
        parser.error('--paper-transitions requires --motion-system and vice versa')
    if not args.paper_transitions:
        return fallback
    adapter = args.motion_system.resolve()
    revision = subprocess.check_output(['git', '-C', str(adapter), 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output(['git', '-C', str(adapter), 'status', '--porcelain', '--untracked-files=all'], text=True).strip()
    if revision != PROFILE['system_sha'] or dirty:
        parser.error('Clean exact motion System required: '+PROFILE['system_sha'])
    return adapter


def version(name, baseline, selected):
    return PROFILE['versions'][name] if selected else baseline


def receipt(adapter):
    return {'enabled': True, 'build_define': DEFINE,
            'system_sha': PROFILE['system_sha'],
            'source_sha256': {name: hashlib.sha256((adapter/name).read_bytes()).hexdigest()
                              for name in PROFILE['sources']},
            'hardware_verified': False}
