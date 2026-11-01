"""Verify complete artifact bytes, inspect all names, expand only selected members."""
import hashlib,zipfile
from pathlib import PurePosixPath

def file_sha256(path):
    digest=hashlib.sha256()
    with open(path,'rb') as stream:
        for chunk in iter(lambda:stream.read(1024*1024),b''):digest.update(chunk)
    return digest.hexdigest()

def selected_archive(path, required, optional=()):
    required=set(required);optional=set(optional)
    with zipfile.ZipFile(path) as archive:
        names=archive.namelist()
        if len(names)!=len(set(names)):raise ValueError('Duplicate archive member')
        for name in names:
            p=PurePosixPath(name)
            if p.is_absolute() or '..' in p.parts or '\\' in name:raise ValueError('Unsafe archive member')
        files={n for n in names if not n.endswith('/')}
        if not required<=files:raise ValueError('Missing required archive members: '+repr(sorted(required-files)))
        return {name:archive.read(name) for name in sorted(required|(optional&files))}
