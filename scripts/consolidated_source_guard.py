#!/usr/bin/env python3
"""Exact blob/mode admission for the approved current-main source composition.

This validates source identity only. It does not replace any behavioral, target,
release-byte, artifact-transport, or hardware qualification.
"""
import argparse
import json
import hashlib
import os
import stat
from pathlib import Path, PurePosixPath
import subprocess

BASE = '6b2047500905f8f45087cb49989103a926fc5c63'
MANIFESTS = ('controls-power', 'signal-capture')
DIRECTORY = 'docs/consolidation/20261022'


def require(value, message):
    if not value:
        raise ValueError(message)


def git(root, *args):
    env = {k:v for k,v in os.environ.items() if not k.startswith('GIT_')}
    env.update(GIT_NO_REPLACE_OBJECTS='1', GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL=os.devnull)
    return subprocess.check_output(['git', '-C', str(root), '--work-tree='+str(Path(root).resolve()),
        '-c', 'core.fsmonitor=false', '-c', 'core.ignoreStat=false',
        '-c', 'core.excludesFile='+os.devnull, *args], text=True, env=env).strip()


def safe_path(path):
    require(isinstance(path, str) and path and not PurePosixPath(path).is_absolute() and
            all(p not in ('', '.', '..') for p in path.split('/')) and '\\' not in path,
            'invalid source path')
    return path


def inventory(root, revision):
    result = {}
    for row in git(root, 'ls-tree', '-r', revision).splitlines():
        info, path = row.split('\t', 1)
        mode, kind, blob = info.split()
        require(kind == 'blob', 'unexpected source tree entry')
        result[path] = {'mode': mode, 'blob': blob}
    return result


def admitted_files(records):
    expected = {}
    metadata = set()
    for name, record in records.items():
        require(name in MANIFESTS and record['base'] == BASE and record['replacement'] == name,
                'composition identity mismatch')
        require(record['frozen_baselines_changed'] is False and record['released'] is False and
                record['hardware_qualified'] is False, 'unsupported composition scope')
        metadata |= {f'{DIRECTORY}/{name}.json', f'{DIRECTORY}/{name}.md'}
        source = record['source_files']
        overrides = record.get('integration_files', {})
        for path in source.keys() | overrides.keys():
            safe_path(path)
            original = source.get(path)
            entry = overrides.get(path, original)
            require(entry['mode'] in ('100644', '100755') and len(entry['blob']) == 40,
                    'invalid blob or mode')
            if path in overrides:
                require(bool(entry.get('reason')) and entry.get('original_blob') ==
                        (original['blob'] if original else None), 'undocumented integration resolution')
            identity = {key: entry[key] for key in ('mode', 'blob')}
            require(path not in expected or expected[path] == identity, 'partitions disagree: '+path)
            expected[path] = identity
    return expected, metadata


def validate_delta(base, current, records):
    expected, metadata = admitted_files(records)
    changed = {p for p in base.keys() | current.keys() if base.get(p) != current.get(p)}
    require(changed - metadata == set(expected), 'missing or unadmitted composition path: '+
            repr(sorted((changed - metadata) ^ set(expected))))
    for path, identity in expected.items():
        require(current.get(path) == identity, 'composition blob/mode mismatch: '+path)
    require(metadata <= current.keys(), 'missing composition metadata')
    return expected


def validate(root):
    root = Path(root).resolve()
    # Candidate builders launched after admission must inherit the same Git
    # interpretation, not redirected repositories or replacement objects.
    for name in list(os.environ):
        if name.startswith('GIT_'):
            del os.environ[name]
    os.environ['GIT_NO_REPLACE_OBJECTS'] = '1'
    require(Path(git(root, 'rev-parse', '--show-toplevel')).resolve() == root, 'redirected Git worktree')
    require(all(line.startswith('H ') for line in git(root, 'ls-files', '-v').splitlines()),
            'assume-unchanged or skip-worktree source entry')
    require(not git(root, 'status', '--porcelain', '--untracked-files=all', '--ignore-submodules=none'),
            'composition checkout must be clean')
    records = {name: json.loads((root/f'{DIRECTORY}/{name}.json').read_text())
               for name in MANIFESTS if (root/f'{DIRECTORY}/{name}.json').is_file()}
    require(records, 'missing composition manifest')
    current = inventory(root, 'HEAD')
    for name, identity in current.items():
        path = root / safe_path(name)
        require(path.resolve().is_relative_to(root), 'source path escapes checkout: '+name)
        mode = path.lstat().st_mode
        require(stat.S_ISREG(mode), 'nonregular working source: '+name)
        actual_mode = '100755' if mode & 0o111 else '100644'
        data = path.read_bytes()
        blob = hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
        require(identity == {'mode':actual_mode, 'blob':blob}, 'working source blob/mode mismatch: '+name)
    expected = validate_delta(inventory(root, BASE), current, records)
    revisions = {}
    for record in records.values():
        inputs = {row['number']: row['head'] for row in record['inputs']}
        for path, entry in record['source_files'].items():
            require(inputs.get(entry['pr']) == entry['commit'], 'source PR/head mismatch: '+path)
            pin = entry['commit']
            if pin not in revisions:
                revisions[pin] = inventory(root, pin)
            require(revisions[pin].get(path) == {k:entry[k] for k in ('blob','mode')},
                    'source provenance mismatch: '+path)
    return {'source': git(root, 'rev-parse', 'HEAD'), 'partitions': sorted(records),
            'admitted_files': len(expected), 'source_heads': sorted(revisions)}


def available(root):
    return any((Path(root)/f'{DIRECTORY}/{name}.json').is_file() for name in MANIFESTS)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    print(json.dumps(validate(parser.parse_args().root), indent=2))


if __name__ == '__main__':
    main()
