"""Validate against the pinned public firmware ABI; no firmware checkout needed."""
import json

def firmware_exports(repo):
    return set(json.loads((repo / 'sdk/firmware-exports.json').read_text()))

def validate_imports(symbol_listing, exports):
    required = set()
    for line in symbol_listing.splitlines():
        fields = line.split()
        if len(fields) >= 8 and fields[4] in {'GLOBAL', 'WEAK'} and fields[6] == 'UND':
            required.add(fields[7])
    missing = sorted(required - exports)
    if missing:
        raise ValueError('ELF imports symbols not exported by pinned firmware: ' + ', '.join(missing))
    return required
