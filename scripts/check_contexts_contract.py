#!/usr/bin/env python3
"""Verify the RF-only enum extension against the pinned public model ABI.

The selected header adds two enum tail values and comments only. Compare every
remaining C token, including all fields, callback types and existing constants.
This is intentionally narrower than a general ABI compatibility checker.
"""
import argparse
from contextlib import contextmanager
import hashlib
from pathlib import Path
import re
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PUBLIC_HEADER_SHA256 = 'b53eedfeb40fda91829c06c956a8bfd4c0630f9894b9b59cc79e07bed40ce4f5'
EXTENSIONS = ('CONTEXTS_MODEL_UNAVAILABLE', 'CONTEXTS_IMPORT_UNAVAILABLE')


def tokens(source):
    # The exact public input is hash-bound below. Preserve string/character
    # literals when removing comments, so a changed literal cannot hide tokens.
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*'
    source = re.sub(pattern, lambda m: ' ' if m[0].startswith(('/',)) else m[0],
                    source, flags=re.S)
    return re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\w+|[^\s]', source)


def verify(selected, public):
    if selected == public:
        return
    if hashlib.sha256(public).hexdigest() != PUBLIC_HEADER_SHA256:
        raise ValueError('Unexpected public System model-header bytes')
    current = tokens(selected.decode('utf-8'))
    for name in EXTENSIONS:
        if current.count(name) != 1:
            raise ValueError('Missing or duplicate RF-only enum extension: ' + name)
        at = current.index(name)
        if at < 1 or current[at - 1:at + 2] != [',', name, '}']:
            raise ValueError('RF-only state must remain an appended implicit enum value: ' + name)
        del current[at - 1:at + 1]
    if current != tokens(public.decode('utf-8')):
        raise ValueError('RF-only header changes the public model contract beyond the two appended states')


@contextmanager
def editor_sdk(system):
    """Use the selected provider's header with the compatible real System UI.

    Quoted sibling includes in PortableContextsClient.h otherwise select the
    older System enum definitions despite the ordinary -I ordering. Preserve
    all System headers and replace only this verified compatible contract.
    """
    public_dir = system / 'lib/PortableApps/include'
    selected = ROOT / 'lib/Contexts/include/ContextsServiceV1.h'
    verify(selected.read_bytes(), (public_dir / selected.name).read_bytes())
    with tempfile.TemporaryDirectory(prefix='contexts-selected-sdk-') as tmp:
        target = Path(tmp) / 'include'
        shutil.copytree(public_dir, target)
        shutil.copyfile(selected, target / selected.name)
        yield target


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--system-apps', required=True, type=Path)
    a = p.parse_args()
    verify((ROOT / 'lib/Contexts/include/ContextsServiceV1.h').read_bytes(),
           (a.system_apps / 'lib/PortableApps/include/ContextsServiceV1.h').read_bytes())
    print('RF-only Contexts contract: exact public ABI tokens plus two appended state values PASS')


if __name__ == '__main__':
    main()
