"""Reviewed full-feature Spectrum evolution, separate from historical evidence."""
import hashlib
import json
from pathlib import Path
import tempfile

from shared_input_parity_reference import MANIFEST, apply_entry

KEY = 'native-target'
BASE = '12c7440c4c2fd9b6b78e10f11a983947c145b346'
PATHS = {'Apps/context_signal_library.inc', 'Apps/context_fingerprint_app.inc'}


def definition():
    data = json.loads(MANIFEST.read_bytes())
    migration = data['references'][KEY]
    names = [entry['path'] for entry in migration['files']]
    if data['schema'] != 1 or migration['baseline_revision'] != BASE or set(names) != PATHS or len(names) != len(PATHS):
        raise ValueError('Wrong reviewed native target evolution')
    if not all(entry.get('rationale') for entry in migration['files']):
        raise ValueError('Missing native target rationale')
    return data, migration


def evolve(relative, before):
    """Unlisted historical files remain byte-for-byte unchanged."""
    _, migration = definition()
    entry = next((item for item in migration['files'] if item['path'] == relative), None)
    if entry is None:
        return before
    with tempfile.TemporaryDirectory(prefix='native-target-evolution-') as directory:
        root = Path(directory)
        path = root/relative
        path.parent.mkdir(parents=True)
        path.write_bytes(before)
        apply_entry(root, entry)
        return path.read_bytes()


def evidence():
    data, migration = definition()
    return {'id': KEY, 'baseline_revision': BASE,
            'reviewed_source_revision': data['reviewed_source_revision'],
            'manifest_sha256': hashlib.sha256(MANIFEST.read_bytes()).hexdigest(),
            'files': [{key: value for key, value in item.items() if key != 'hunks'}
                      for item in migration['files']]}
