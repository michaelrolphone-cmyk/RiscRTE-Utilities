#!/usr/bin/env python3
"""Publish a qualified immutable standalone Alarms development release."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile
from prepare_native_alarms_release import TAG, VERSION, expected_manifest, require, read, validate_controls, validate_target, CONTROL_PROFILE, checked_path
import build_native_alarms_qualification as recipe

DIRECT = ('alarms.elf','alarms.json','command.json','alarms-build-record.json',
          'qualification.json','elf-layout.json','elf-memory-admission.json')
REQUIRED = {*DIRECT,'publication.json','SHA256SUMS',TAG+'.zip'}


def validate(root, source):
    app = root/'app'; record = read(app/'alarms-build-record.json')
    target = read(root/'target/qualification.json')
    require(record['source_commit'] == source and target['utilities_commit'] == source and
            not record['working_tree_dirty'] and not target['working_tree_dirty'], 'Wrong source identity')
    require(record['tag'] == TAG and record['version'] == VERSION and
            read(app/'alarms.json') == expected_manifest(), 'Wrong release identity')
    require(record['external_git_pins'] == recipe.PINS == target['external_git_pins'] and
            record['required_capabilities'] == expected_manifest()['requires'], 'Wrong dependency contract')
    require(record['hardware_qualified'] is False and record['firmware_replacement'] is False and
            record['legacy_manifest_unchanged'] is True, 'Unsupported qualification claim')
    require(record['target_receipt_sha256'] == recipe.digest((root/'target/qualification.json').read_bytes()), 'Target receipt changed')
    require(record['controls_profile_sha256'] == recipe.digest((recipe.ROOT/CONTROL_PROFILE).read_bytes()), 'Control contract changed')
    require(record['controls'] == validate_controls(root,target), 'Control receipts changed')
    modules = validate_target(root,target)
    native = modules[('alarms','native')]
    require(native['sha256'] == record['elf_sha256'] and native['size_bytes'] == record['elf_size_bytes'] and
            native['build_defines'] == recipe.define_arguments(True), 'Native target mismatch')
    require(set(native['exports']) == recipe.EXPORTS and set(native['imports']) <= recipe.IMPORTS, 'Import/export drift')
    for filename,key in [('alarms.elf','elf_sha256'),('alarms.json','manifest_sha256'),
                         ('command.json','command_sha256'),('elf-layout.json','layout_sha256'),
                         ('elf-memory-admission.json','admission_sha256')]:
        require(recipe.digest((app/filename).read_bytes()) == record[key], 'Changed release input: '+filename)
    elf = (app/'alarms.elf').read_bytes(); recipe.elf_contract(elf)
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
    validate_memory(root,record,modules)
    require(record['source_files'], 'Missing source evidence')
    for module in modules.values():
        if module['mode'] == 'native':
            for identity, sha in module['source_sha256'].items():
                owner, relative = identity.split(':',1)
                require(record['source_files'].get('source/'+owner+'/'+relative) == sha, 'Source bundle omits production input: '+identity)
    for name, sha in record['source_files'].items():
        path = Path(name)
        require(not path.is_absolute() and '..' not in path.parts and path.parts[0] == 'source', 'Invalid source evidence path')
        require(recipe.digest(checked_path(root,str(path)).read_bytes()) == sha, 'Changed source evidence: '+name)
    return record


def validate_memory(root, record, modules):
    require(set(record['memory_admissions']) == set(recipe.APPLICATIONS), 'Missing dual-app memory admission')
    for app in recipe.APPLICATIONS:
        bound = record['memory_admissions'][app]
        layout_path = root/'admission'/(app+'-elf-layout.json')
        admission_path = root/'admission'/(app+'-elf-memory-admission.json')
        require(recipe.digest(layout_path.read_bytes()) == bound['layout_sha256'] and
                recipe.digest(admission_path.read_bytes()) == bound['admission_sha256'], 'Changed memory evidence: '+app)
        layout, admission = read(layout_path), read(admission_path)
        require(layout['sha256'] == modules[(app,'native')]['sha256'] and layout['file_size_bytes'] == modules[(app,'native')]['size_bytes'] and
                layout['linked_device_memory_fit_qualified'] is False, 'Memory layout mismatch: '+app)
        require(admission['app'] == app and admission['runtime_commit'] == recipe.PINS['runtime'] and
                admission['elf_sha256'] == layout['sha256'] and admission['layout_sha256'] == bound['layout_sha256'] and
                admission['required_capabilities'] == expected_manifest()['requires'] and
                admission['linked_device_memory_fit_qualified'] is False and admission['target_instructions_executed'] is False,
                'Unbound memory admission: '+app)
        require([x['sanitized'] for x in admission['runs']] == [False,True], 'Incomplete production admission: '+app)
        require(admission['runtime_source_sha256'] and admission['shared_inspector_sha256'], 'Missing production inspector provenance')
        for run in admission['runs']:
            result = run['result']
            require(all(result.get(k) is True for k in ('ordinary_application_admitted','missing_import_rejected','unsafe_state_rejected','driver_role_rejected'))
                    and result['alignment_placements_checked'] > 0 and result['data_allocation_request_bytes'] ==
                    layout['section_loader']['data_allocation_request_bytes'], 'Failed production admission: '+app)
        for relative, sha in admission['runtime_source_sha256'].items():
            require(recipe.digest(checked_path(root/'admission/runtime',relative).read_bytes()) == sha, 'Runtime admission source changed')
        require(recipe.digest((root/'admission/report_native_settings_memory.py').read_bytes()) == admission['shared_inspector_sha256'],
                'Production inspector changed')


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
    for filename,key in [('alarms.elf','elf_sha256'),('alarms.json','manifest_sha256'),
                         ('command.json','command_sha256'),('elf-layout.json','layout_sha256'),
                         ('elf-memory-admission.json','admission_sha256'),('qualification.json','target_receipt_sha256')]:
        require(recipe.digest((folder/filename).read_bytes()) == record[key], 'Published qualified artifact differs: '+filename)
    require(read(folder/'alarms-build-record.json') == record, 'Published build record differs')
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
            shutil.copyfile(root/'target/qualification.json' if name == 'qualification.json' else root/'app'/name,out/name)
        publication = {'schema':1,'tag':TAG,'source_commit':args.source_sha,'qualification':'software-only',
                       'hardware_qualified':False,'firmware_replacement':False,
                       'elf_sha256':record['elf_sha256'],'manifest_sha256':record['manifest_sha256'],
                       'build_record_sha256':recipe.digest((root/'app/alarms-build-record.json').read_bytes())}
        (out/'publication.json').write_text(json.dumps(publication,indent=2)+'\n')
        with zipfile.ZipFile(out/(TAG+'.zip'),'w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
            for path in sorted(root.rglob('*')):
                if path.is_file(): archive.write(path,path.relative_to(root))
            archive.write(out/'publication.json','publication.json')
        (out/'SHA256SUMS').write_text(''.join(recipe.digest(p.read_bytes())+'  '+p.name+'\n' for p in sorted(out.iterdir()) if p.is_file()))
        notes = ('Standalone Native Alarms 0.3.0 development/software qualification. Exact source: '+args.source_sha+
                 '. Requires all 15 manifest providers, deployment-owned bindings, preferences namespace 1 and sufficient loader memory. '
                 'Physical-device fit, installation-path compatibility and hardware behavior are unqualified. '
                 'The optional app.catalog grant is not added. Countdown controls are evidence only. '
                 'Legacy Alarms and the .59 firmware/payload are unchanged. Source, license notices and qualification evidence are included.')
        gh('release','create',TAG,'--repo',args.repository,'--target',args.source_sha,'--draft','--prerelease',
           '--title','Alarms Native 0.3.0 development release','--notes',notes)
        gh('release','upload',TAG,'--repo',args.repository,*[str(out/name) for name in sorted(REQUIRED)])
        with tempfile.TemporaryDirectory() as downloaded:
            gh('release','download',TAG,'--repo',args.repository,'--dir',downloaded)
            verify_release_assets(Path(downloaded),record)
        gh('release','edit',TAG,'--repo',args.repository,'--draft=false')
        print('Published '+TAG+' at '+args.source_sha)

if __name__ == '__main__':main()
