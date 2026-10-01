#!/usr/bin/env python3
"""Check the immutable SDK snapshot and classify three-way app source drift.

The optional Reader checkout is read via git objects only, never overwritten.
This command never synchronizes files: external-only edits and conflicts require
review rather than a blind copy from upstream.
"""
import argparse
import hashlib
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]


def git_blob(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def classify(base, local, upstream):
    if local == upstream:
        return 'unchanged' if local == base else 'converged'
    if local == base:
        return 'upstream-only'
    if upstream == base:
        return 'external-only'
    return 'conflict'


def check_sdk(root=ROOT):
    lock = json.loads((root / 'sdk/baseline.json').read_text())
    for item in lock['files']:
        data = (root / item['path']).read_bytes()
        if hashlib.sha256(data).hexdigest() != item['sha256'] or git_blob(data) != item['git_blob']:
            raise ValueError('SDK snapshot changed without an audited rebaseline: ' + item['path'])
    if hashlib.sha256((root / 'sdk/firmware-exports.json').read_bytes()).hexdigest() != lock['exports_sha256']:
        raise ValueError('Pinned firmware export inventory changed')
    return lock


def audit(reader=None, ref=None, root=ROOT):
    manifest = json.loads((root / 'utilities-manifest.json').read_text())
    commit = None
    if reader:
        commit = subprocess.check_output(['git', '-C', str(reader), 'rev-parse', '--verify', (ref or 'HEAD') + '^{commit}'], text=True).strip()
    result = []
    for app in manifest['apps']:
        inputs = [(app['source_path'], app['upstream_source_sha']),
                  (app['manifest_path'], app['upstream_manifest_sha'])]
        inputs += [(item['path'], item['upstream_blob']) for item in app.get('additional_sources', [])]
        for path, base in inputs:
            local = git_blob((root / path).read_bytes())
            upstream = None
            if reader:
                upstream = subprocess.check_output(['git', '-C', str(reader), 'rev-parse', f'{commit}:{path}'], text=True).strip()
            result.append({'id': app['id'], 'path': path, 'baseline_blob': base,
                           'external_blob': local, 'upstream_blob': upstream,
                           'state': classify(base, local, upstream) if upstream else ('unchanged' if base == local else 'external-only-unverified')})
    return {'reader_commit': commit, 'files': result}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reader', type=pathlib.Path)
    parser.add_argument('--ref', help='Explicit Reader commit/ref to compare (default HEAD)')
    parser.add_argument('--output', type=pathlib.Path)
    args = parser.parse_args()
    check_sdk()
    report = audit(args.reader, args.ref)
    text = json.dumps(report, indent=2) + '\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
    else:
        print(text, end='')
