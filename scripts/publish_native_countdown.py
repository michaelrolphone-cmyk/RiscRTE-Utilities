#!/usr/bin/env python3
"""Publish only the exact, independently versioned native Countdown package."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile
import build_native_alarms_qualification as recipe
from prepare_native_countdown_release import (TAG, VERSION, PACKAGE, EXTRA_SOURCE,
    expected_manifest, qualified_record, require, read, checked_path)

DIRECT = ('countdown.elf','countdown.json','command.json','countdown-build-record.json',
          'qualification.json','elf-layout.json','elf-memory-admission.json')
REQUIRED = {*DIRECT,'publication.json','SHA256SUMS',TAG+'.zip'}


def validate(root, source):
    package = root/PACKAGE
    record = read(package/'countdown-build-record.json')
    require(record == qualified_record(root, source), 'Countdown release record differs from qualified evidence')
    require((package/'countdown-build-record.json').read_bytes() ==
            json.dumps(record, indent=2).encode()+b'\n', 'Countdown build record encoding changed')
    require(read(package/'countdown.json') == expected_manifest(), 'Wrong Countdown release identity')
    for filename,key in [('countdown.elf','elf_sha256'),('countdown.json','manifest_sha256'),
                         ('command.json','command_sha256'),('elf-layout.json','layout_sha256'),
                         ('elf-memory-admission.json','admission_sha256')]:
        require(recipe.digest((package/filename).read_bytes()) == record[key], 'Changed Countdown release input: '+filename)
    require((package/'countdown.elf').read_bytes() == (root/'target/native/countdown.elf').read_bytes(),
            'Countdown target bytes differ')
    admission = read(package/'elf-memory-admission.json')
    expected = read(root/'admission/countdown-elf-memory-admission.json')
    expected['manifest_sha256'] = record['manifest_sha256']
    require(admission == expected, 'Countdown manifest is not bound to production admission')
    require(admission['app'] == 'countdown', 'Wrong admitted application')
    for name, sha in record['source_files'].items():
        require(recipe.digest(checked_path(root,name).read_bytes()) == sha, 'Changed Countdown source evidence: '+name)
    require(read(root/'source/utilities/Apps/native/countdown.json') == expected_manifest(),
            'Countdown source manifest differs')
    return record


def publication_record(record):
    return {'schema':1, 'tag':TAG, 'source_commit':record['source_commit'],
            'qualification':'software-only', 'hardware_qualified':False,
            'firmware_replacement':False, 'elf_sha256':record['elf_sha256'],
            'manifest_sha256':record['manifest_sha256'],
            'build_record_sha256':recipe.digest(json.dumps(record,indent=2).encode()+b'\n')}


def verify_existing(published, tagged, record):
    require(published['tag'] == TAG and published['source_commit'] == tagged == record['source_commit'], 'Immutable tag/source mismatch')
    require(published.get('elf_sha256') == record['elf_sha256'] and
            published.get('manifest_sha256') == record['manifest_sha256'] and
            published.get('build_record_sha256') == recipe.digest(json.dumps(record,indent=2).encode()+b'\n'),
            'Existing release differs from the qualified ELF/manifest; choose a new version')


def verify_release_assets(folder, record):
    """Verify downloaded immutable assets and the enclosed source/control bundle."""
    require(REQUIRED <= {p.name for p in folder.iterdir() if p.is_file()}, 'Release assets missing')
    sums = {}
    for line in (folder/'SHA256SUMS').read_text().splitlines():
        sha, name = line.split('  ',1)
        require(name in REQUIRED-{'SHA256SUMS'} and name not in sums, 'Invalid release checksum entry')
        require(len(sha) == 64 and all(c in '0123456789abcdef' for c in sha), 'Invalid release checksum')
        sums[name] = sha
    require(set(sums) == REQUIRED-{'SHA256SUMS'}, 'Incomplete release checksums')
    for name, sha in sums.items():
        require(recipe.digest((folder/name).read_bytes()) == sha, 'Published asset changed: '+name)
    for filename,key in [('countdown.elf','elf_sha256'),('countdown.json','manifest_sha256'),
                         ('command.json','command_sha256'),('elf-layout.json','layout_sha256'),
                         ('elf-memory-admission.json','admission_sha256'),('qualification.json','target_receipt_sha256')]:
        require(recipe.digest((folder/filename).read_bytes()) == record[key], 'Published qualified artifact differs: '+filename)
    require(read(folder/'countdown-build-record.json') == record, 'Published build record differs')
    require(read(folder/'publication.json') == publication_record(record), 'Published release identity differs')
    with tempfile.TemporaryDirectory() as temporary:
        extracted = Path(temporary)
        with zipfile.ZipFile(folder/(TAG+'.zip')) as archive:
            for item in archive.infolist():
                checked_path(extracted,item.filename)
                require((item.external_attr >> 16) & 0o170000 != 0o120000, 'Archive symlink is not admitted')
            archive.extractall(extracted)
        require(validate(extracted,record['source_commit']) == record, 'Published evidence bundle differs')


def gh(*args,check=True):
    return subprocess.run(['gh',*args],check=check,capture_output=True,text=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact',type=Path,required=True)
    parser.add_argument('--source-sha',required=True)
    parser.add_argument('--repository',required=True)
    action = parser.add_mutually_exclusive_group()
    action.add_argument('--validate-only',action='store_true')
    action.add_argument('--publish',action='store_true',help='Explicitly publish after separate integration approval')
    args = parser.parse_args(); root = args.artifact.resolve()
    record = validate(root,args.source_sha)
    if not args.publish:
        print('Standalone publication inputs PASS');return
    require(gh('api','repos/'+args.repository+'/commits/'+args.source_sha,'--jq','.sha').stdout.strip() == args.source_sha,
            'Exact source is not present in the requested remote repository')
    prior = gh('release','view',TAG,'--repo',args.repository,'--json','tagName,isDraft,isPrerelease,assets',check=False)
    if prior.returncode == 0:
        r = json.loads(prior.stdout)
        require(not r['isDraft'] and r['isPrerelease'] and REQUIRED <= {x['name'] for x in r['assets']},
                'Existing immutable release is incomplete; manual review required')
        with tempfile.TemporaryDirectory() as temporary:
            gh('release','download',TAG,'--repo',args.repository,'--dir',temporary)
            published = read(Path(temporary)/'publication.json')
            verify_release_assets(Path(temporary),record)
        tagged = gh('api','repos/'+args.repository+'/commits/'+TAG,'--jq','.sha').stdout.strip()
        verify_existing(published,tagged,record)
        print('Existing immutable release verified at '+tagged+'; current source remains '+args.source_sha);return
    require('404' in prior.stderr or 'not found' in prior.stderr.lower(), 'Release lookup failed')
    tag = gh('api','repos/'+args.repository+'/git/ref/tags/'+TAG,check=False)
    if tag.returncode == 0:
        require(gh('api','repos/'+args.repository+'/commits/'+TAG,'--jq','.sha').stdout.strip() == args.source_sha,
                'Existing tag belongs to another source')
    else:
        require('404' in tag.stderr,'Tag lookup failed')
    with tempfile.TemporaryDirectory() as temporary:
        out = Path(temporary)
        for name in DIRECT:
            shutil.copyfile(root/'target/qualification.json' if name == 'qualification.json' else root/PACKAGE/name,out/name)
        publication = publication_record(record)
        (out/'publication.json').write_text(json.dumps(publication,indent=2)+'\n')
        with zipfile.ZipFile(out/(TAG+'.zip'),'w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
            for path in sorted(root.rglob('*')):
                if path.is_file(): archive.write(path,path.relative_to(root))
            archive.write(out/'publication.json','publication.json')
        (out/'SHA256SUMS').write_text(''.join(recipe.digest(p.read_bytes())+'  '+p.name+'\n' for p in sorted(out.iterdir()) if p.is_file()))
        notes = ('Standalone Native Countdown 0.2.0 development/software qualification. Exact source: '+args.source_sha+
                 '. Requires all 15 manifest providers, deployment-owned bindings, preferences namespace 1 and sufficient loader memory. '
                 'Physical-device fit, installation-path compatibility and hardware behavior are unqualified. '
                 'The optional app.catalog grant is not added. Alarms controls are evidence only; no Alarms release is changed. '
                 'Legacy Countdown, Alarms and the .59 firmware/payload are unchanged. Source, license notices and qualification evidence are included.')
        gh('release','create',TAG,'--repo',args.repository,'--target',args.source_sha,'--draft','--prerelease',
           '--title','Countdown Native 0.2.0 development release','--notes',notes)
        gh('release','upload',TAG,'--repo',args.repository,*[str(out/name) for name in sorted(REQUIRED)])
        with tempfile.TemporaryDirectory() as downloaded:
            gh('release','download',TAG,'--repo',args.repository,'--dir',downloaded)
            verify_release_assets(Path(downloaded),record)
        gh('release','edit',TAG,'--repo',args.repository,'--draft=false')
        print('Published '+TAG+' at '+args.source_sha)

if __name__ == '__main__':main()
