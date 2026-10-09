#!/usr/bin/env python3
"""Verify the exact source selected by the Watch image reconstruction."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'docs/watch-recovery'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def main():
    record = json.loads((BASE / 'custody.json').read_bytes())
    require(record['schema'] == 1 and record['original_whole_tree_equivalence'] is False,
            'Incorrect recovery claim')
    bundle = BASE / record['bundle']['file']
    require(bundle.parent == BASE and bundle.is_file() and not bundle.is_symlink(), 'Invalid bundle path')
    raw = bundle.read_bytes()
    require(len(raw) == record['bundle']['size_bytes'] and
            hashlib.sha256(raw).hexdigest() == record['bundle']['sha256'], 'Bundle digest differs')
    require(len(record['production_inputs']) == 135, 'Incomplete production input record')
    with tempfile.TemporaryDirectory(prefix='utilities-recovery-') as directory:
        def git(*args):
            return subprocess.check_output(['git', '-C', directory, *args], stderr=subprocess.STDOUT)
        git('init', '--bare')
        git('fetch', str(ROOT), record['prerequisite'])
        git('bundle', 'verify', str(bundle))
        git('fetch', str(bundle), 'HEAD:refs/recovery/watch')
        require(git('rev-parse', 'refs/recovery/watch').decode().strip() == record['commit'], 'Commit differs')
        require(git('rev-parse', record['commit'] + '^{tree}').decode().strip() == record['tree'], 'Tree differs')
        for name, digest in record['production_inputs'].items():
            require(hashlib.sha256(git('show', record['commit'] + ':' + name)).hexdigest() == digest,
                    'Production input differs: ' + name)
    print('Exact Utilities recovery commit, tree and all 135 production inputs verified')


if __name__ == '__main__':
    main()
