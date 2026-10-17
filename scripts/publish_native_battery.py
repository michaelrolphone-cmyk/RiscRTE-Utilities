#!/usr/bin/env python3
"""Publish a qualified immutable standalone Battery development release."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile
from prepare_native_battery_release import TAG, VERSION, expected_manifest, require, read, validate_controls
import build_native_battery_qualification as recipe

DIRECT = ('battery.elf','battery.json','command.json','battery-build-record.json',
          'qualification.json','elf-layout.json','elf-memory-admission.json')
REQUIRED = {*DIRECT,'publication.json','SHA256SUMS',TAG+'.zip'}


def validate(root, source):
    app = root/'app'; record = read(app/'battery-build-record.json')
    target = read(root/'target/qualification.json')
    require(record['source_commit'] == source and target['utilities_commit'] == source and
            not record['working_tree_dirty'] and not target['working_tree_dirty'], 'Wrong source identity')
    require(record['tag'] == TAG and record['version'] == VERSION and
            read(app/'battery.json') == expected_manifest(), 'Wrong release identity')
    require(record['external_git_pins'] == recipe.PINS == target['external_git_pins'] and
            record['required_capabilities'] == expected_manifest()['requires'], 'Wrong dependency contract')
    require(record['hardware_qualified'] is False and record['firmware_replacement'] is False and
            record['legacy_manifest_unchanged'] is True, 'Unsupported qualification claim')
    require(record['target_receipt_sha256'] == recipe.digest((root/'target/qualification.json').read_bytes()), 'Target receipt changed')
    require(record['controls'] == validate_controls(root,target), 'Control receipts changed')
    native = next(m for m in target['modules'] if m['mode'] == 'native')
    require(native['sha256'] == record['elf_sha256'] and native['size_bytes'] == record['elf_size_bytes'] and
            native['build_defines'] == recipe.define_arguments(True), 'Native target mismatch')
    require(set(native['exports']) == recipe.EXPORTS and set(native['imports']) <= recipe.IMPORTS, 'Import/export drift')
    for filename,key in [('battery.elf','elf_sha256'),('battery.json','manifest_sha256'),
                         ('command.json','command_sha256'),('elf-layout.json','layout_sha256'),
                         ('elf-memory-admission.json','admission_sha256')]:
        require(recipe.digest((app/filename).read_bytes()) == record[key], 'Changed release input: '+filename)
    elf = (app/'battery.elf').read_bytes(); recipe.elf_contract(elf)
    require(len(elf) == record['elf_size_bytes'], 'ELF length mismatch')
    command = read(app/'command.json')
    require([x for x in command if x.startswith('-D')] == recipe.define_arguments(True), 'Compiled selectors changed')
    layout = read(app/'elf-layout.json'); admission = read(app/'elf-memory-admission.json')
    require(layout['sha256'] == record['elf_sha256'] and layout['file_size_bytes'] == len(elf) and
            layout['linked_device_memory_fit_qualified'] is False, 'Layout mismatch')
    require(admission['runtime_commit'] == recipe.PINS['runtime'] and
            admission['elf_sha256'] == record['elf_sha256'] and admission['manifest_sha256'] == record['manifest_sha256'] and
            admission['layout_sha256'] == record['layout_sha256'] and
            admission['linked_device_memory_fit_qualified'] is False and admission['target_instructions_executed'] is False,
            'Unbound production admission')
    require([x['sanitized'] for x in admission['runs']] == [False,True], 'Incomplete production admission')
    for run in admission['runs']:
        r = run['result']
        require(all(r.get(x) is True for x in ('ordinary_application_admitted','missing_import_rejected',
                    'unsafe_state_rejected','driver_role_rejected')) and r['alignment_placements_checked'] > 0 and
                r['data_allocation_request_bytes'] == layout['section_loader']['data_allocation_request_bytes'], 'Failed admission')
    require(record['source_files'], 'Missing source evidence')
    for name, sha in record['source_files'].items():
        path = Path(name)
        require(not path.is_absolute() and '..' not in path.parts and path.parts[0] == 'source', 'Invalid source evidence path')
        require(recipe.digest((root/path).read_bytes()) == sha, 'Changed source evidence: '+name)
    return record


def verify_existing(published, tagged, record):
    require(published['tag'] == TAG and published['source_commit'] == tagged, 'Immutable tag mismatch')
    require(published.get('elf_sha256') == record['elf_sha256'] and
            published.get('manifest_sha256') == record['manifest_sha256'],
            'Existing release differs from the qualified ELF/manifest; choose a new version')


def gh(*args,check=True):
    return subprocess.run(['gh',*args],check=check,capture_output=True,text=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact',type=Path,required=True)
    parser.add_argument('--source-sha',required=True)
    parser.add_argument('--repository',required=True)
    parser.add_argument('--validate-only',action='store_true')
    args = parser.parse_args(); root = args.artifact.resolve()
    record = validate(root,args.source_sha)
    if args.validate_only:
        print('Standalone publication inputs PASS');return
    prior = gh('release','view',TAG,'--repo',args.repository,'--json','tagName,isDraft,isPrerelease,assets',check=False)
    if prior.returncode == 0:
        r = json.loads(prior.stdout)
        require(not r['isDraft'] and r['isPrerelease'] and REQUIRED <= {x['name'] for x in r['assets']},
                'Existing immutable release is incomplete; manual review required')
        with tempfile.TemporaryDirectory() as temporary:
            gh('release','download',TAG,'--repo',args.repository,'--pattern','publication.json','--dir',temporary)
            published = read(Path(temporary)/'publication.json')
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
            shutil.copyfile(root/'target/qualification.json' if name == 'qualification.json' else root/'app'/name,out/name)
        publication = {'schema':1,'tag':TAG,'source_commit':args.source_sha,'qualification':'software-only',
                       'hardware_qualified':False,'firmware_replacement':False,
                       'elf_sha256':record['elf_sha256'],'manifest_sha256':record['manifest_sha256']}
        (out/'publication.json').write_text(json.dumps(publication,indent=2)+'\n')
        with zipfile.ZipFile(out/(TAG+'.zip'),'w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
            for path in sorted(root.rglob('*')):
                if path.is_file(): archive.write(path,path.relative_to(root))
            archive.write(out/'publication.json','publication.json')
        (out/'SHA256SUMS').write_text(''.join(recipe.digest(p.read_bytes())+'  '+p.name+'\n' for p in sorted(out.iterdir()) if p.is_file()))
        notes = ('Standalone Native Battery1.2.0 development/software qualification. Exact source: '+args.source_sha+
                 '. Requires all15 manifest providers, deployment-owned bindings, preferences namespace1 and sufficient loader memory. '
                 'Physical-device fit, installation-path compatibility and hardware behavior are unqualified. '
                 'The optional app.catalog grant is not added; color BLE entry remains deferred. '
                 'Legacy Battery and the .59 firmware/payload are unchanged. Source, license notices and qualification evidence are included.')
        gh('release','create',TAG,'--repo',args.repository,'--target',args.source_sha,'--draft','--prerelease',
           '--title','Battery Native1.2.0 development release','--notes',notes)
        gh('release','upload',TAG,'--repo',args.repository,*[str(out/name) for name in sorted(REQUIRED)])
        gh('release','edit',TAG,'--repo',args.repository,'--draft=false')
        print('Published '+TAG+' at '+args.source_sha)

if __name__ == '__main__':main()
