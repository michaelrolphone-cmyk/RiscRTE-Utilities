#!/usr/bin/env python3
"""Verify focused Points source custody without modifying a checkout."""
import hashlib
import json
from pathlib import Path
import zipfile

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'docs/evidence/points-catalog'


def main():
    record=json.loads((BASE/'source-custody.json').read_bytes())
    assert record['schema']==1
    assert record['qualified_original_source']=='9bd572791a8304194ceb2b7542fc9cbd124e911b'
    assert record['baseline_original_source']=='0d3c1fbb4488549a47459279cbcaadb7f9fbd064'
    item=record['baseline_archive'];path=BASE/item['file']
    assert path.parent==BASE and path.name=='baseline-source.zip' and not path.is_symlink()
    raw=path.read_bytes();assert len(raw)==item['size_bytes'] and hashlib.sha256(raw).hexdigest()==item['sha256']
    with zipfile.ZipFile(path) as z:
        assert len(z.namelist())==len(record['baseline_members']) and set(z.namelist())==set(record['baseline_members'])
        for name,expected in record['baseline_members'].items():
            assert not Path(name).is_absolute() and '..' not in Path(name).parts
            raw=z.read(name);assert len(raw)==expected['size_bytes'] and hashlib.sha256(raw).hexdigest()==expected['sha256']
    for name,digest in record['selected_source_hashes'].items():
        path=ROOT/name;assert path.is_file() and not path.is_symlink()
        assert hashlib.sha256(path.read_bytes()).hexdigest()==digest,name
    print('Focused Points production source and original flag-off baseline custody verified')


if __name__=='__main__':main()
