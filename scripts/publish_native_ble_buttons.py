#!/usr/bin/env python3
"""Publish only the exact, independently versioned native BLE Buttons package."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile
import build_native_ble_buttons_qualification as recipe
from prepare_native_ble_buttons_release import (TAG, VERSION, PACKAGE, RECORD,
    expected_manifest, qualified_record, require, read, checked_path)

DIRECT = ('ble_buttons.elf','ble_buttons.json','command.json','ble_buttons-build-record.json',
          'qualification.json','elf-layout.json','elf-memory-admission.json')
REQUIRED = {*DIRECT,'publication.json','SHA256SUMS',TAG+'.zip'}


def validate(root, source):
    package = root/PACKAGE
    record = read(package/'ble_buttons-build-record.json')
    require(record == qualified_record(root, source), 'BLE Buttons release record differs from qualified evidence')
    require((package/'ble_buttons-build-record.json').read_bytes() ==
            json.dumps(record, indent=2).encode()+b'\n', 'BLE Buttons build record encoding changed')
    require(read(package/'ble_buttons.json') == expected_manifest(), 'Wrong BLE Buttons release identity')
    for filename,key in [('ble_buttons.elf','elf_sha256'),('ble_buttons.json','manifest_sha256'),
                         ('command.json','command_sha256'),('elf-layout.json','layout_sha256'),
                         ('elf-memory-admission.json','admission_sha256')]:
        require(recipe.sha((package/filename).read_bytes()) == record[key], 'Changed BLE Buttons release input: '+filename)
    require((package/'ble_buttons.elf').read_bytes() == (root/'target/native/ble_buttons.elf').read_bytes(),
            'BLE Buttons target bytes differ')
    for name, sha in record['source_files'].items():
        require(recipe.sha(checked_path(root,name).read_bytes()) == sha, 'Changed BLE Buttons source evidence: '+name)
    require(read(root/'source/utilities/Apps/native/ble_buttons.json') == expected_manifest(),
            'BLE Buttons source manifest differs')
    return record


def publication_record(record):
    return {'schema':1, 'tag':TAG, 'source_commit':record['source_commit'],
            'qualification':'software-only', 'hardware_qualified':False,
            'firmware_replacement':False, 'elf_sha256':record['elf_sha256'],
            'manifest_sha256':record['manifest_sha256'],
            'build_record_sha256':recipe.sha(json.dumps(record,indent=2).encode()+b'\n')}


def verify_existing(published, tagged, record):
    require(published['tag'] == TAG and published['source_commit'] == tagged == record['source_commit'], 'Immutable tag/source mismatch')
    require(published.get('elf_sha256') == record['elf_sha256'] and
            published.get('manifest_sha256') == record['manifest_sha256'] and
            published.get('build_record_sha256') == recipe.sha(json.dumps(record,indent=2).encode()+b'\n'),
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
        require(recipe.sha((folder/name).read_bytes()) == sha, 'Published asset changed: '+name)
    for filename,key in [('ble_buttons.elf','elf_sha256'),('ble_buttons.json','manifest_sha256'),
                         ('command.json','command_sha256'),('elf-layout.json','layout_sha256'),
                         ('elf-memory-admission.json','admission_sha256'),('qualification.json','target_receipt_sha256')]:
        require(recipe.sha((folder/filename).read_bytes()) == record[key], 'Published qualified artifact differs: '+filename)
    require(read(folder/'ble_buttons-build-record.json') == record, 'Published build record differs')
    require(read(folder/'publication.json') == publication_record(record), 'Published release identity differs')
    with tempfile.TemporaryDirectory() as temporary:
        extracted = Path(temporary)
        with zipfile.ZipFile(folder/(TAG+'.zip')) as archive:
            names = set()
            require(sum(i.file_size for i in archive.infolist()) <= 512*1024*1024, 'Archive expansion budget exceeded')
            for item in archive.infolist():
                require(item.filename not in names, 'Duplicate archive member')
                names.add(item.filename)
                checked_path(extracted,item.filename)
                require((item.external_attr >> 16) & 0o170000 != 0o120000, 'Archive symlink is not admitted')
            archive.extractall(extracted)
        require(validate(extracted,record['source_commit']) == record, 'Published evidence bundle differs')
        require(read(extracted/'publication.json') == publication_record(record),
                'Archived publication identity differs')


def stage_assets(root, out, record):
    require(not out.resolve().is_relative_to(root.resolve()), 'Asset output must be outside the evidence bundle')
    require(not out.exists() or not any(out.iterdir()), 'Asset staging directory must be empty')
    out.mkdir(parents=True,exist_ok=True)
    for name in DIRECT:
        shutil.copyfile(root/'target/qualification.json' if name == 'qualification.json' else root/PACKAGE/name,out/name)
    publication = publication_record(record)
    (out/'publication.json').write_text(json.dumps(publication,indent=2)+'\n')
    with zipfile.ZipFile(out/(TAG+'.zip'),'w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
        for path in sorted(root.rglob('*')):
            if path.is_symlink():
                require(path.resolve().is_relative_to(root.resolve()) and path.resolve().is_file(),
                        'Bundle symlink escapes evidence or is not a file')
                # The target builder coalesces identical SDK headers in-bundle.
                # ZIP stores regular dereferenced bytes, never link metadata.
            if path == root/'publication.json':
                require(read(path) == publication, 'Existing bundle publication differs')
                continue
            if path.is_file(): archive.write(path,path.relative_to(root))
        archive.write(out/'publication.json','publication.json')
    (out/'SHA256SUMS').write_text(''.join(recipe.sha(p.read_bytes())+'  '+p.name+'\n' for p in sorted(out.iterdir()) if p.is_file()))
    verify_release_assets(out,record)


def gh(*args,check=True):
    return subprocess.run(['gh',*args],check=check,capture_output=True,text=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact',type=Path,required=True)
    parser.add_argument('--source-sha',required=True)
    parser.add_argument('--repository',required=True)
    parser.add_argument('--stage-assets',type=Path,help='Create and locally verify the ten assets without publishing')
    action = parser.add_mutually_exclusive_group()
    action.add_argument('--validate-only',action='store_true')
    action.add_argument('--publish',action='store_true',help='Explicit immutable publication; default is validation only')
    args = parser.parse_args(); root = args.artifact.resolve()
    record = validate(root,args.source_sha)
    if args.stage_assets:
        require(not args.publish, 'Stage-assets and publish are separate actions')
        stage_assets(root,args.stage_assets.resolve(),record)
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
    with tempfile.TemporaryDirectory() as temporary:
        out = Path(temporary)
        stage_assets(root,out,record)
        # Draft creation does not create a new Git tag. Bind it explicitly
        # before creating the draft; never overwrite or repoint an existing tag.
        tag = gh('api','repos/'+args.repository+'/git/ref/tags/'+TAG,check=False)
        if tag.returncode != 0:
            require('404' in tag.stderr,'Tag lookup failed')
            gh('api','repos/'+args.repository+'/git/refs','--method','POST',
               '-f','ref=refs/tags/'+TAG,'-f','sha='+args.source_sha)
        require(gh('api','repos/'+args.repository+'/commits/'+TAG,'--jq','.sha').stdout.strip() == args.source_sha,
                'Existing tag belongs to another source')
        notes = ('Standalone Native BLE Buttons 0.1.30-dev1 software qualification. Exact source: '+args.source_sha+
                 '. Requires all 16 manifest capability/API pairs, deployment-owned bindings, compatible public ABI and sufficient loader memory. '
                 'Production structural/import/lifecycle/alignment admission passes; device PSRAM fit, target instruction execution, '
                 'installation-path compatibility and hardware behavior remain unqualified. '
                 'Legacy BLE Buttons bindings and frozen .64 firmware/payload remain unchanged. '
                 'Source closure, license notices and all qualification controls are included.')
        gh('release','create',TAG,'--repo',args.repository,'--target',args.source_sha,'--verify-tag','--draft','--prerelease',
           '--title','BLE Buttons Native 0.1.30-dev1 development release','--notes',notes)
        gh('release','upload',TAG,'--repo',args.repository,*[str(out/name) for name in sorted(REQUIRED)])
        with tempfile.TemporaryDirectory() as downloaded:
            gh('release','download',TAG,'--repo',args.repository,'--dir',downloaded)
            verify_release_assets(Path(downloaded),record)
        tagged = gh('api','repos/'+args.repository+'/commits/'+TAG,'--jq','.sha').stdout.strip()
        verify_existing(publication_record(record),tagged,record)
        gh('release','edit',TAG,'--repo',args.repository,'--draft=false')
        final = json.loads(gh('release','view',TAG,'--repo',args.repository,
                             '--json','tagName,isDraft,isPrerelease,assets').stdout)
        require(final['tagName'] == TAG and not final['isDraft'] and final['isPrerelease'] and
                REQUIRED <= {x['name'] for x in final['assets']}, 'Final published release differs')
        with tempfile.TemporaryDirectory() as downloaded:
            gh('release','download',TAG,'--repo',args.repository,'--dir',downloaded)
            verify_release_assets(Path(downloaded),record)
        print('Published '+TAG+' at '+args.source_sha)

if __name__ == '__main__':main()
