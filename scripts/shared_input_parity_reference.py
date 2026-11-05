"""Exact reviewed source evolution; historical inputs are never rewritten."""
import difflib
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'test/shared_input_parity/evolution-v1.json'


def blob_hash(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def checked_path(root, name):
    path = Path(name)
    if path.is_absolute() or '..' in path.parts or not path.parts or path.parts[0] not in ('Apps', 'test'):
        raise ValueError('Invalid evolution path: ' + name)
    result = root / path
    if not result.resolve().is_relative_to(root.resolve()) or result.is_symlink():
        raise ValueError('Evolution path escapes reference: ' + name)
    return result


def apply_entry(root, entry):
    path = checked_path(root, entry['path'])
    before = path.read_bytes()
    if blob_hash(before) != entry['before_git_blob_sha1']:
        raise ValueError('Historical input hash mismatch: ' + entry['path'])
    lines = before.decode('utf-8').splitlines(keepends=True)
    previous = len(lines) + 1
    for hunk in reversed(entry['hunks']):
        start, old, new = hunk['start'], hunk['old'], hunk['new']
        if not isinstance(start, int) or start < 0 or start + len(old) > len(lines) or start + len(old) > previous:
            raise ValueError('Invalid/overlapping hunk: ' + entry['path'])
        if lines[start:start + len(old)] != old:
            raise ValueError('Exact hunk mismatch: ' + entry['path'])
        lines[start:start + len(old)] = new
        previous = start
    after = ''.join(lines).encode()
    if blob_hash(after) != entry['after_git_blob_sha1']:
        raise ValueError('Reviewed output hash mismatch: ' + entry['path'])
    path.write_bytes(after)


class Reference:
    def __init__(self, baseline, key):
        self.temporary = tempfile.TemporaryDirectory(prefix='shared-input-reference-')
        self.path = Path(self.temporary.name)
        data = json.loads(MANIFEST.read_text())
        if data['schema'] != 1:
            raise ValueError('Unknown evolution schema')
        migration = data['references'][key]
        baseline = Path(baseline).resolve()
        if (baseline / '.git').exists():
            repository = baseline
            actual = subprocess.check_output(
                ['git', '-C', str(baseline), 'rev-parse', 'HEAD'], text=True).strip()
        else:
            repository = ROOT
            actual = (baseline / '.historical-revision').read_text().strip()
        if actual != migration['baseline_revision']:
            raise ValueError('Wrong immutable historical baseline: ' + actual)
        # Exact committed sources exclude working-tree edits and untracked headers.
        # Native qualification archives its baseline from ROOT's full Git history.
        archive = subprocess.check_output(
            ['git', '-C', str(repository), 'archive', actual, 'Apps', 'lib', 'test'])
        with tarfile.open(fileobj=io.BytesIO(archive)) as stream:
            for member in stream:
                name = Path(member.name)
                if name.is_absolute() or '..' in name.parts:
                    raise ValueError('Invalid archive path')
                target = self.path / name
                if member.isdir():
                    target.mkdir(parents=True, exist_ok=True)
                elif member.isfile():
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(stream.extractfile(member).read())
                else:
                    raise ValueError('Non-regular historical archive entry: ' + member.name)
        names = [entry['path'] for entry in migration['files']]
        if len(names) != len(set(names)):
            raise ValueError('Duplicate evolution file')
        for entry in migration['files']:
            if not entry.get('rationale'):
                raise ValueError('Missing reviewed rationale')
            apply_entry(self.path, entry)
        self.evidence = {
            'id': key, 'baseline_revision': actual,
            'reviewed_source_revision': data['reviewed_source_revision'],
            'manifest_sha256': hashlib.sha256(MANIFEST.read_bytes()).hexdigest(),
            'files': [{k: v for k, v in entry.items() if k != 'hunks'}
                      for entry in migration['files']]}


def compare_tokens(before, after, output, label):
    if before != after:
        output = Path(output)
        output.mkdir(parents=True, exist_ok=True)
        diff = ''.join(difflib.unified_diff(
            before.decode().splitlines(True), after.decode().splitlines(True),
            fromfile='reviewed-reference', tofile='candidate'))
        (output / 'parity-mismatch.diff').write_text(label + '\n' + diff)
        raise AssertionError(label + '; see ' + str(output / 'parity-mismatch.diff'))


# C preprocessing token boundaries matter: ++ is not + +, and 1.0 is not 1 . 0.
TOKEN_RE = re.compile(
    r'(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|(?:u|U|L)?\'(?:\\.|[^\'\\])*\'|'
    r'(?:[0-9]|\.[0-9])(?:[eEpP][+-]|[A-Za-z_0-9.])*|[A-Za-z_$][A-Za-z_0-9$]*|'
    r'%:%:|>>=|<<=|\.\.\.|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||'
    r'\*=|/=|%=|\+=|-=|&=|\^=|\|=|##|<:|:>|<%|%>|%:|[^\s]')

def tokenize(data):
    return '\n'.join(TOKEN_RE.findall(data)).encode()

def source_snapshot(tree):
    result = {}
    for directory in ('Apps', 'lib', 'scripts', 'test', 'tests'):
        root = Path(tree) / directory
        if root.exists():
            for path in sorted(root.rglob('*')):
                if path.is_file() and '__pycache__' not in path.parts and path.suffix != '.pyc':
                    result[str(path.relative_to(tree))] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result

def verify_stable(tree, before):
    if source_snapshot(tree) != before:
        raise AssertionError('Production, helper or fixture source changed during qualification')
